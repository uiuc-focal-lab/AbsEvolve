#include "dual_quad_constraints_solver.hpp"
#include "utils.hpp"

#include <algorithm>
#include <dlfcn.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

//
// Constants
//
const double INF = std::numeric_limits<double>::infinity();
const int BOUND_PENALTY_WEIGHT = 1000;
const int OPT_WEIGHT = 100;

//
// Helper methods and struct
//
bool is_torch_cuda_available()
{
    void* handle = dlopen("libtorch_cuda.so", RTLD_LAZY);
    if (!handle) {
        return false;  // CUDA library is missing
    }
    dlclose(handle);
    return torch::cuda::is_available();
}

double round_down_if_not_integral(double val) {
    return (std::floor(val) == val) ? val : std::floor(val);
}

double get_dbl_val_from_tensor(torch::Tensor dbl_tensor)
{
    double dbl_val = dbl_tensor.item<double>();
    return round_down_if_not_integral(dbl_val);
}

struct ProblemTensors
{
    torch::Tensor A;  // Shape: [num_rel_constraints, num_vars]
    torch::Tensor b;  // Shape: [num_rel_constraints]
    torch::Tensor bounds; // Shape: [num_vars, 2]

    //
    // Tensors specific to the objectives
    //

    // Quadratic part specific tensors
    torch::Tensor single_quadratic_probs_tensor;
    torch::Tensor single_var_inds_tensor;
    torch::Tensor double_quadratic_probs_tensor;
    torch::Tensor double_var_inds_tensor;

    // Linear part specific tensors
    torch::Tensor C;
    torch::Tensor cons;

    // Indices of combinations that are trivially unbounded
    std::set<int> trivially_unbounded_indices;
};

ProblemTensors extract_problem_tensors(const QuadConstraintsProblem& quad_constraints_problem,
                                       std::map<int, int> var_num_to_ind_map,
                                       bool parse_adaptive_learning_mask)
{
    ProblemTensors tensors;

    //
    // Parse all constraints in A and b
    //
    tensors.A = torch::zeros({quad_constraints_problem.constraints_map.size(), quad_constraints_problem.variables.size()}, torch::dtype(torch::kDouble));
    tensors.b = torch::zeros({quad_constraints_problem.constraints_map.size()}, torch::dtype(torch::kDouble));
    int var, var_ind, constraint_idx;
    double b_val, coeff;
    constraint_idx = 0;
    for (const auto& entry : quad_constraints_problem.constraints_map) {
        // Inequality constant
        b_val = entry.second;

        // Traverse the constraint
        const auto& tup = entry.first;
        for (auto it = tup.begin(); it != tup.end(); it++) {
            var = (*it).var;
            var_ind = var_num_to_ind_map[var];
            coeff = (*it).coeff;

            if (var == -1) {
                b_val -= coeff;
            }
            else {
                tensors.A.index_put_({constraint_idx, var_ind}, tensors.A.index({constraint_idx, var_ind}) + coeff);
            }
        }

        // Set the final inequality constant
        tensors.b.index_put_({constraint_idx}, tensors.b.index({constraint_idx}) + b_val);

        constraint_idx += 1;
    }

    //
    // Parse the bounds constraints
    //
    tensors.bounds = torch::full({tensors.A.size(1), 2}, INF, torch::dtype(torch::kDouble));
    tensors.bounds.index_put_({torch::indexing::Slice(), 0}, -INF); // set lower bounds to -inf
    for (const auto& [var, bounds] : quad_constraints_problem.var_bounds_map) {
        double lb = bounds.first;
        double ub = bounds.second;
        var_ind = var_num_to_ind_map[var];

        tensors.bounds.index_put_({var_ind, 0}, lb);
        tensors.bounds.index_put_({var_ind, 1}, ub);
    }

    //
    // Parse the objective combinations specific data while also filtering out the trivially unbounded objectives
    //
    int comb_idx = 0;
    int all_idx = 0;
    int var1, var2, var1_ind, var2_ind;
    double coeff1, coeff2;

    // Intermediate storage
    bool is_unb = false;
    double cons = 0;
    torch::Tensor c_row;
    std::vector<torch::Tensor> single_quad_prob_rows, single_var_ind_rows, double_quad_prob_rows, double_var_ind_rows;

    // Final data structs
    std::vector<double> cons_vec;
    std::vector<torch::Tensor> C_rows_final, single_quad_prob_rows_final, single_var_ind_rows_final, double_quad_prob_rows_final, double_var_ind_rows_final;

    for (const auto& combination : quad_constraints_problem.combinations_to_solve) {
        // Reset the intermediate values
        cons = 0;
        is_unb = false;
        c_row = torch::zeros({quad_constraints_problem.variables.size()}, torch::kDouble);
        single_quad_prob_rows.clear();
        single_var_ind_rows.clear();
        double_quad_prob_rows.clear();
        double_var_ind_rows.clear();

        // Parse the combination
        for (const auto& coeff_var_pair : combination) {
            var1 = coeff_var_pair.first.var;
            var1_ind = -1;
            if (var1 != -1) {
                var1_ind = var_num_to_ind_map[var1];
            }
            coeff1 = coeff_var_pair.first.coeff;

            var2 = coeff_var_pair.second.var;
            var2_ind = -1;
            if (var2 != -1) {
                var2_ind = var_num_to_ind_map[var2];
            }
            coeff2 = coeff_var_pair.second.coeff;

            coeff = coeff1 * coeff2;

            if (coeff == 0) continue;

            if (var1_ind != -1 && var2_ind != -1) {
                if (var1_ind == var2_ind) {
                    // Single Quadratic Case

                    bool is_neg_inf_x = tensors.bounds[var1_ind][0].item<double>() == -INF;
                    bool is_pos_inf_x = tensors.bounds[var1_ind][1].item<double>() == INF;

                    // Checking trivial unboundedness
                    // A negative and either lx -inf or ux inf
                    if (coeff < 0 && (is_neg_inf_x || is_pos_inf_x))
                    {
                        is_unb = true;
                        tensors.trivially_unbounded_indices.insert(all_idx);
                        break;
                    }
                    
                    // Add single quad specific data to intermediate data struct for now
                    single_quad_prob_rows.push_back(
                        torch::tensor(
                            {
                                static_cast<double>(comb_idx),
                                static_cast<double>(var1_ind),
                                coeff,
                                tensors.bounds[var1_ind][0].item<double>(), tensors.bounds[var1_ind][1].item<double>()
                            },
                            torch::kDouble
                        )
                    );

                    single_var_ind_rows.push_back(
                        torch::tensor(
                            {
                                static_cast<double>(comb_idx),
                                static_cast<double>(var1_ind),
                            },
                            torch::kDouble
                        )
                    );
                } else {
                    // Double Quadratic Case

                    bool is_neg_inf_x = tensors.bounds[var1_ind][0].item<double>() == -INF;
                    bool is_pos_inf_x = tensors.bounds[var1_ind][1].item<double>() == INF;
                    bool is_neg_inf_y = tensors.bounds[var2_ind][0].item<double>() == -INF;
                    bool is_pos_inf_y = tensors.bounds[var2_ind][1].item<double>() == INF;
                    
                    // Checking trivial unboundedness
                    // 1. A negative: lx, ly -inf or ux, uy inf
                    // 2. A positive: lx -inf and uy inf or vice versa
                    if (coeff < 0) {
                        if ((is_neg_inf_x && is_neg_inf_y) || (is_pos_inf_x && is_pos_inf_y)) {
                            is_unb = true;
                            tensors.trivially_unbounded_indices.insert(all_idx);
                            break;
                        }
                    } else {
                        if ((is_neg_inf_x && is_pos_inf_y) || (is_neg_inf_y && is_pos_inf_x)) {
                            is_unb = true;
                            tensors.trivially_unbounded_indices.insert(all_idx);
                            break;
                        }
                    }

                    // Add double quad specific data to intermediate data struct for now
                    double_quad_prob_rows.push_back(
                        torch::tensor(
                            {
                            static_cast<double>(comb_idx),
                            static_cast<double>(var1_ind),
                            static_cast<double>(var2_ind),
                            coeff,
                            tensors.bounds[var1_ind][0].item<double>(), tensors.bounds[var1_ind][1].item<double>(),
                            tensors.bounds[var2_ind][0].item<double>(), tensors.bounds[var2_ind][1].item<double>()
                            },
                            torch::kDouble
                        )
                    );

                    double_var_ind_rows.push_back(
                        torch::tensor(
                            {
                                static_cast<double>(comb_idx),
                                static_cast<double>(var1_ind),
                                static_cast<double>(var2_ind),
                            },
                            torch::kDouble
                        )
                    );
                }
            }
            else if (var1_ind != -1) {
                c_row.index_put_({var1_ind}, c_row.index({var1_ind}) + coeff);
            }
            else if (var2_ind != -1) {
                c_row.index_put_({var2_ind}, c_row.index({var2_ind}) + coeff);
            }
            else {
                cons += coeff;
            }
        }

        if (!is_unb) {
            // If not unbounded, then store into final data structs
            cons_vec.push_back(cons);
            C_rows_final.push_back(c_row);
            single_quad_prob_rows_final.insert(single_quad_prob_rows_final.end(), single_quad_prob_rows.begin(), single_quad_prob_rows.end());
            single_var_ind_rows_final.insert(single_var_ind_rows_final.end(), single_var_ind_rows.begin(), single_var_ind_rows.end());
            double_quad_prob_rows_final.insert(double_quad_prob_rows_final.end(), double_quad_prob_rows.begin(), double_quad_prob_rows.end());
            double_var_ind_rows_final.insert(double_var_ind_rows_final.end(), double_var_ind_rows.begin(), double_var_ind_rows.end());
            comb_idx += 1;
        }

        all_idx += 1;
    }

    // Use the final data structs to create the tensors
    if (C_rows_final.size() > 0) {
        tensors.C = torch::stack(C_rows_final);
        tensors.cons = torch::from_blob(cons_vec.data(), {cons_vec.size()}, torch::dtype(torch::kDouble)).clone();
    }

    if (single_quad_prob_rows_final.size() > 0) {
        tensors.single_quadratic_probs_tensor = torch::stack(single_quad_prob_rows_final);
        tensors.single_var_inds_tensor = torch::stack(single_var_ind_rows_final);
    }

    if (double_quad_prob_rows_final.size() > 0) {
        tensors.double_quadratic_probs_tensor = torch::stack(double_quad_prob_rows_final);
        tensors.double_var_inds_tensor = torch::stack(double_var_ind_rows_final);
    }

    return tensors;
}

//
// Double quadratic case specific methods
//

torch::Tensor double_quadratic_obj_val_corner_computer(torch::Tensor bound1, torch::Tensor bound2, bool is_bound1_lb, bool is_bound2_lb)
{
    //
    // Helps in computing the objective value component for each corner.
    //

    // Start with all infs
    torch::Tensor answer = torch::full({bound1.size(0)}, INF, torch::kDouble); 

    // Case where it can lead to -inf
    torch::Tensor unbound_potential = 
        (torch::isinf(bound1) & (is_bound1_lb ? bound2 > 0 : bound2 < 0 )) | 
        (torch::isinf(bound2) & (is_bound2_lb ? bound1 > 0 : bound1 < 0 ));
    answer = torch::where(unbound_potential, -INF, answer);

    // Case where finite answers are there: either both finite or one zero
    torch::Tensor both_finite = torch::isfinite(bound1) & torch::isfinite(bound2);
    torch::Tensor fin_candidate = torch::where(both_finite, bound1 * bound2, 0);
    answer = torch::where(both_finite, torch::minimum(fin_candidate, answer), answer);

    torch::Tensor mult_zero = (bound1 == 0 | bound2 == 0);
    answer = torch::where(mult_zero, torch::minimum(answer, torch::zeros_like(answer)), answer);

    return answer;
}

std::tuple<
    torch::Tensor, torch::Tensor,
    torch::Tensor, torch::Tensor>
    double_quadratic_loss_corner_computer(torch::Tensor bound1, torch::Tensor bound2, bool is_bound1_lb, bool is_bound2_lb)
{
    //
    // Helps in computing the structures that help computing loss
    //

    //
    // First compute the unbound potential mask and loss for that
    // 1. For the mask, it is the two cases where -inf could occur but which could be fixed (that is other bound is finite).
    // 2. In that case, loss is there to fix the other coefficient.
    //
    torch::Tensor final_unbound_mask, unbound_mask1, masked_bound1, unbound_mask2, masked_bound2, unbound_loss;
    unbound_loss = torch::zeros({bound1.size(0)}, torch::kDouble);

    unbound_mask1 = (torch::isinf(bound1) & torch::isfinite(bound2) & (is_bound1_lb ? bound2 > 0 : bound2 < 0 ));
    masked_bound2 = torch::where(unbound_mask1, bound2, torch::zeros_like(bound2));
    unbound_loss = torch::where(unbound_mask1, unbound_loss + torch::pow(masked_bound2, 2), unbound_loss);

    unbound_mask2 = (torch::isinf(bound2) & torch::isfinite(bound1) & (is_bound2_lb ? bound1 > 0 : bound1 < 0 ));
    masked_bound1 = torch::where(unbound_mask2, bound1, torch::zeros_like(bound1));
    unbound_loss = torch::where(unbound_mask2, unbound_loss + torch::pow(masked_bound1, 2), unbound_loss);

    final_unbound_mask = unbound_mask1 | unbound_mask2;

    //
    // Compute the finite component: two cases for finite, either both finite or one zero
    //
    torch::Tensor final_fin_mask, fin_mask1, fin_mask2, safe_prod, fin_val;
    fin_val = torch::zeros({bound1.size(0)}, torch::kDouble);

    fin_mask1 = torch::isfinite(bound1) & torch::isfinite(bound2);
    masked_bound1 = torch::where(fin_mask1, bound1, torch::zeros_like(bound1));
    masked_bound2 = torch::where(fin_mask1, bound2, torch::zeros_like(bound2));
    safe_prod = masked_bound1 * masked_bound2;
    fin_val = torch::where(fin_mask1, safe_prod, fin_val);

    fin_mask2 = (bound1 == 0 | bound2 == 0);
    fin_val = torch::where(fin_mask2 & ~fin_mask1,
                           0, 
                           torch::where(fin_mask2 & fin_mask1, torch::minimum(fin_val, torch::zeros_like(fin_val)), fin_val));

    final_fin_mask = fin_mask1 | fin_mask2;

    return {final_unbound_mask, unbound_loss, final_fin_mask, fin_val};
}

torch::Tensor get_double_quadratic_min_obj_vals_for_params(
    torch::Tensor& double_quadratic_probs_tensor,
    const torch::Tensor& var_coeffs,
    int num_combs
)
{
    //
    // min_{x, y} Axy + Bx + Cy = (Ax + C)(y + B/A) - BC/A
    // Find interval bounds for the two brackets and then find the
    // interval bounds for the pair
    //

    // Extract the needed tensors
    torch::Tensor A = double_quadratic_probs_tensor.index({torch::indexing::Slice(), 3});
    torch::Tensor lx = double_quadratic_probs_tensor.index({torch::indexing::Slice(), 4});
    torch::Tensor ux = double_quadratic_probs_tensor.index({torch::indexing::Slice(), 5});
    torch::Tensor ly = double_quadratic_probs_tensor.index({torch::indexing::Slice(), 6});
    torch::Tensor uy = double_quadratic_probs_tensor.index({torch::indexing::Slice(), 7});
    torch::Tensor B = var_coeffs.index({torch::indexing::Slice(), 0}); 
    torch::Tensor C = var_coeffs.index({torch::indexing::Slice(), 1});

    // Get updated bounds
    torch::Tensor eff_lx = torch::where(A > 0, A * lx, A * ux) + C;
    torch::Tensor eff_ux = torch::where(A > 0, A * ux, A * lx) + C;
    torch::Tensor eff_ly = ly + B / A;
    torch::Tensor eff_uy = uy + B / A;

    // Start answers with inf
    torch::Tensor answer = torch::full({A.size(0)}, INF, torch::kDouble); 
    torch::Tensor both_finite, mult_zero, fin_candidate, unbound_potential;

    // First combination: eff_lx, eff_ly
    answer = torch::minimum(double_quadratic_obj_val_corner_computer(eff_lx, eff_ly, true, true), answer);

    // Second combination: eff_lx, eff_uy
    answer = torch::minimum(double_quadratic_obj_val_corner_computer(eff_lx, eff_uy, true, false), answer);

    // Third combination: eff_ux, eff_ly
    answer = torch::minimum(double_quadratic_obj_val_corner_computer(eff_ux, eff_ly, false, true), answer);

    // Fourth combination: eff_ux, eff_uy
    answer = torch::minimum(double_quadratic_obj_val_corner_computer(eff_ux, eff_uy, false, false), answer);

    // Add the constant part
    answer = answer - (B * C) / A;

    // Collating the answers
    torch::Tensor result = torch::zeros({num_combs}, torch::kDouble);
    torch::Tensor indices = double_quadratic_probs_tensor.index({torch::indexing::Slice(), 0}).to(torch::kLong);
    result.index_add_(0, indices, answer);

    return result;
}

torch::Tensor get_double_quadratic_loss_vals_for_params(
    torch::Tensor& double_quadratic_probs_tensor,
    const torch::Tensor& var_coeffs,
    int num_combs,
    int bound_penalty_weight, int opt_weight
)
{
    // Extract the needed tensors
    torch::Tensor A = double_quadratic_probs_tensor.index({torch::indexing::Slice(), 3});
    torch::Tensor lx = double_quadratic_probs_tensor.index({torch::indexing::Slice(), 4});
    torch::Tensor ux = double_quadratic_probs_tensor.index({torch::indexing::Slice(), 5});
    torch::Tensor ly = double_quadratic_probs_tensor.index({torch::indexing::Slice(), 6});
    torch::Tensor uy = double_quadratic_probs_tensor.index({torch::indexing::Slice(), 7});
    torch::Tensor B = var_coeffs.index({torch::indexing::Slice(), 0}); 
    torch::Tensor C = var_coeffs.index({torch::indexing::Slice(), 1});

    // Get updated bounds
    torch::Tensor eff_lx = torch::where(A > 0, A * lx, A * ux) + C;
    torch::Tensor eff_ux = torch::where(A > 0, A * ux, A * lx) + C;
    torch::Tensor eff_ly = ly + B / A;
    torch::Tensor eff_uy = uy + B / A;

    // Collect loss calculation components from the four corners
    auto [ub_mask1, ub_loss1, fin_mask1, fin_val1] = double_quadratic_loss_corner_computer(eff_lx, eff_ly, true, true);
    auto [ub_mask2, ub_loss2, fin_mask2, fin_val2] = double_quadratic_loss_corner_computer(eff_lx, eff_uy, true, false);
    auto [ub_mask3, ub_loss3, fin_mask3, fin_val3] = double_quadratic_loss_corner_computer(eff_ux, eff_ly, false, true);
    auto [ub_mask4, ub_loss4, fin_mask4, fin_val4] = double_quadratic_loss_corner_computer(eff_ux, eff_uy, false, false);

    // Final unbounded cases mask and their loss
    torch::Tensor final_ub_mask = ub_mask1 | ub_mask2 | ub_mask3 | ub_mask4;
    torch::Tensor final_ub_loss = ub_loss1 + ub_loss2 + ub_loss3 + ub_loss4;

    // Correctly collecting the final val cases, taking the minimum only when something is defined, else it is 0 which
    // is just a placeholder
    torch::Tensor final_obj_val = fin_val1;
    torch::Tensor final_obj_mask = fin_mask1;

    // Getting values from fin_val2
    final_obj_val = torch::where(fin_mask2 & ~final_obj_mask,
                                 fin_val2, 
                                 torch::where(fin_mask2 & final_obj_mask, torch::minimum(final_obj_val, fin_val2), final_obj_val));
    final_obj_mask = final_obj_mask | fin_mask2;

    // Getting values from fin_val3
    final_obj_val = torch::where(fin_mask3 & ~final_obj_mask,
                                 fin_val3, 
                                 torch::where(fin_mask3 & final_obj_mask, torch::minimum(final_obj_val, fin_val3), final_obj_val));
    final_obj_mask = final_obj_mask | fin_mask3;

    // Getting values from fin_val4
    final_obj_val = torch::where(fin_mask4 & ~final_obj_mask,
                                 fin_val4, 
                                 torch::where(fin_mask4 & final_obj_mask, torch::minimum(final_obj_val, fin_val4), final_obj_val));
    final_obj_mask = final_obj_mask | fin_mask4;

    // Final loss: if unbounded use that else the objective value
    torch::Tensor final_loss = torch::where(
        final_ub_mask,
        bound_penalty_weight * final_ub_loss,
        -1 * opt_weight * final_obj_val
    );

    // Collating the answers
    torch::Tensor result = torch::zeros({num_combs}, torch::kDouble);
    torch::Tensor indices = double_quadratic_probs_tensor.index({torch::indexing::Slice(), 0}).to(torch::kLong);
    result.index_add_(0, indices, final_loss);

    return result;
}

//
// Single quadratic case specific methods
//
torch::Tensor get_single_quadratic_min_obj_vals_for_params(
    torch::Tensor& single_quadratic_probs_tensor,
    const torch::Tensor& var_coeffs,
    int num_combs   
)
{
    //
    // Computes the minimized objective values for single quadratic case
    //
    // If a is positive, the mimimum would be finite as either we will have some finite bound
    // and if no bounds present, then the global minimizer is there.
    //
    // If a is negative, we will surely have both the bounds as the cases where
    // one or both bounds are missing are already filtered.
    //

    // Extract the needed tensors
    torch::Tensor a  = single_quadratic_probs_tensor.index({torch::indexing::Slice(), 2});
    torch::Tensor lb = single_quadratic_probs_tensor.index({torch::indexing::Slice(), 3});
    torch::Tensor ub = single_quadratic_probs_tensor.index({torch::indexing::Slice(), 4});
    torch::Tensor b  = var_coeffs.squeeze(1);

    // Compute the global optimizer, whether it is in the range and is_coeff_pos mask
    torch::Tensor x_star = -b / (2 * a);
    torch::Tensor xstar_in_range = (x_star >= lb) & (x_star <= ub);
    torch::Tensor is_coeff_pos = a > 0; 

    // Candidate 1:
    // -> If A < 0 -> lower bound (always finite)
    // -> If A > 0 and x_star in range -> x_star
    // -> If A > 0 and xstar not in range -> lb if it is finite else ub
    torch::Tensor candidate1 = torch::where(torch::isfinite(lb), lb, ub);
    candidate1 = torch::where((is_coeff_pos & xstar_in_range), x_star, candidate1);

    // Candidate 2:
    // -> If A < 0 -> upper bound (always finite)
    // -> If A > 0 and x_star in range -> x_star
    // -> If A > 0 and xstar not in range -> ub if it is finite else lb
    torch::Tensor candidate2 = torch::where(torch::isfinite(ub), ub, lb);
    candidate2 = torch::where((is_coeff_pos & xstar_in_range), x_star, candidate2);

    // objective at the two candidates
    torch::Tensor f_1 = a * candidate1 * candidate1 + b * candidate1;
    torch::Tensor f_2 = a * candidate2 * candidate2 + b * candidate2;

    // Take min of two candidate values
    torch::Tensor f_min = torch::minimum(f_1, f_2);

    // Collating the answers
    torch::Tensor result = torch::zeros({num_combs}, torch::kDouble);
    torch::Tensor indices = single_quadratic_probs_tensor.index({torch::indexing::Slice(), 0}).to(torch::kLong);
    result.index_add_(0, indices, f_min);

    return result;
}

torch::Tensor get_single_quadratic_loss_vals_for_params(
    torch::Tensor& single_quadratic_probs_tensor,
    const torch::Tensor& var_coeffs,
    int num_combs,
    int bound_penalty_weight, int opt_weight
)
{
    //
    // Computes the loss for the single quadratic case:
    //
    // Its value is always finite so the loss is just the negative of the objective values
    //
    torch::Tensor min_obj_vals = get_single_quadratic_min_obj_vals_for_params(single_quadratic_probs_tensor, var_coeffs, num_combs);

    return -1 * opt_weight * min_obj_vals;
}

//
// Linear part specific methods
//
torch::Tensor get_linear_min_obj_vals_for_params(
    torch::Tensor& effective_c,
    torch::Tensor& var_bounds,
    int num_combs
)
{
    //
    // Compute => min_{V} effective_c * V
    //

    //
    // Get the lower and upper variable bounds and broadcast them to match effective_c: [num_combs, num_vars]
    //
    torch::Tensor lb = var_bounds.index({torch::indexing::Slice(), 0});
    torch::Tensor ub = var_bounds.index({torch::indexing::Slice(), 1});
    torch::Tensor lb_broadcast = lb.unsqueeze(0).expand_as(effective_c);
    torch::Tensor ub_broadcast = ub.unsqueeze(0).expand_as(effective_c);

    //
    // Pick the bounds to use based on the sign of effective_c
    // As we need to minimize: for pos coeffs, pick lower bound and
    // for neg coeffs, pick upper bound.
    //
    torch::Tensor pos_mask = effective_c > 0;
    torch::Tensor picked_bounds = torch::where(pos_mask, lb_broadcast, ub_broadcast);
    
    // Zero out values where effective_c == 0
    picked_bounds = torch::where(effective_c == 0, torch::zeros_like(picked_bounds), picked_bounds);

    //
    // Compute the final answer
    //
    torch::Tensor opt_val = (effective_c * picked_bounds).sum(1);

    return opt_val;
}

torch::Tensor get_linear_loss_vals_for_params(
    torch::Tensor& effective_c,
    torch::Tensor& var_bounds,
    int num_combs,
    int bound_penalty_weight, int opt_weight
)
{
    // In this method, we need to find the loss:
    // 1. If the objective values can be found, then the loss is negative of that
    // as we need to find the max lower bounds
    // 2. For the cases where objective value is -infinite because of the picked bounds,
    // have a loss that will lead the solver to fixing the "problematic" coefficients
    //

    //
    // Get the lower and upper variable bounds and broadcast them to match effective_c: [num_combs, num_vars]
    //
    torch::Tensor lb = var_bounds.index({torch::indexing::Slice(), 0});
    torch::Tensor ub = var_bounds.index({torch::indexing::Slice(), 1});
    torch::Tensor lb_broadcast = lb.unsqueeze(0).expand_as(effective_c);
    torch::Tensor ub_broadcast = ub.unsqueeze(0).expand_as(effective_c);

    //
    // Pick the bounds to use based on the sign of effective_c
    //
    torch::Tensor pos_mask = effective_c > 0;
    torch::Tensor zero_mask = effective_c == 0;

    //
    // Pick the bounds to use based on the sign of effective_c
    // As we need to minimize: for pos coeffs, pick lower bound and
    // for neg coeffs, pick upper bound.
    //
    torch::Tensor picked_bounds = torch::where(pos_mask, lb_broadcast, ub_broadcast);
    picked_bounds = torch::where(zero_mask, torch::zeros_like(picked_bounds), picked_bounds);

    //
    // Compute the optimal value for the specified lambdas
    // As this is the loss for normal cases, we replace the inf bounds with 0 as these
    // cases are handled separately below
    // 
    torch::Tensor non_inf_mask = torch::isfinite(picked_bounds);
    torch::Tensor safe_picked_bounds = torch::where(non_inf_mask, picked_bounds, torch::zeros_like(picked_bounds));
    torch::Tensor opt_val = (effective_c * safe_picked_bounds).sum(1);

    //
    // Loss computation for the error cases
    //
    torch::Tensor inf_bound_picked_cases_mask = torch::isinf(picked_bounds);
    torch::Tensor inf_bound_picked_combs_mask = inf_bound_picked_cases_mask.any(1);

    torch::Tensor effective_c_sq = torch::pow(effective_c, 2);
    torch::Tensor inf_bound_loss = (effective_c_sq * inf_bound_picked_cases_mask.to(effective_c_sq.scalar_type())).sum(1);

    //
    // Final Loss: For cases with inf bound multiplication, use the inf bound loss, else use -1 * opt_val to get the best 
    // (highest lower) bound
    //
    torch::Tensor final_loss = torch::where(inf_bound_picked_combs_mask,
                                            bound_penalty_weight * inf_bound_loss,
                                            -1 * opt_weight * opt_val);

    return final_loss;
}

//
// Combined methods
//
std::pair<torch::Tensor, torch::Tensor> get_effective_C_and_lambda_b(
    torch::Tensor& A,
    torch::Tensor& b,
    torch::Tensor& C,
    torch::Tensor& single_var_inds_tensor,
    torch::Tensor& double_var_inds_tensor,
    const torch::Tensor& single_quad_params,
    const torch::Tensor& double_quad_params,
    const torch::Tensor& lambdas
)
{
    // Lambda * B
    torch::Tensor lamb;
    if (lambdas.defined()) {
        lamb = torch::matmul(lambdas, b);
    }
    else {
        lamb = torch::zeros({C.size(0)}, torch::dtype(torch::kDouble));
    }

    //
    // Effective C
    //

    // First compute (lambda*A + C)
    torch::Tensor lamA_plus_C;
    if (lambdas.defined()) {
        lamA_plus_C = torch::matmul(lambdas, A) + C;
    }
    else {
        lamA_plus_C = C.clone();
    }

    //
    // Update lamA_plus_C to remove the parts that are in single_quad_params and double_quad_params
    //
    torch::Tensor rows, cols, vals;

    // Update using the single quad params
    if (single_quad_params.defined()) {
        rows = single_var_inds_tensor.index({torch::indexing::Slice(), 0}).to(torch::kLong);
        cols = single_var_inds_tensor.index({torch::indexing::Slice(), 1}).to(torch::kLong);
        vals = single_quad_params.squeeze();
        lamA_plus_C.index_put_({rows, cols}, -vals, true);
    }

    // Update using the double quad params
    if (double_quad_params.defined()) {
        rows = double_var_inds_tensor.index({torch::indexing::Slice(), 0}).to(torch::kLong);
        cols = double_var_inds_tensor.index({torch::indexing::Slice(), 1}).to(torch::kLong);
        vals = double_quad_params.index({torch::indexing::Slice(), 0}).squeeze();
        lamA_plus_C.index_put_({rows, cols}, -vals, true);

        cols = double_var_inds_tensor.index({torch::indexing::Slice(), 2}).to(torch::kLong);
        vals = double_quad_params.index({torch::indexing::Slice(), 1}).squeeze();
        lamA_plus_C.index_put_({rows, cols}, -vals, true);
    }

    return {lamA_plus_C, lamb};
}

torch::Tensor get_obj_vals_for_params(
    struct ProblemTensors& prob_tensors,
    const torch::Tensor& single_quad_params,
    const torch::Tensor& double_quad_params,
    const torch::Tensor& lambdas
)
{
    // 
    // Get effective C and lambda b
    //
    auto [effective_c, lamb] = get_effective_C_and_lambda_b(prob_tensors.A, prob_tensors.b, prob_tensors.C,
                                                            prob_tensors.single_var_inds_tensor,
                                                            prob_tensors.double_var_inds_tensor,
                                                            single_quad_params, double_quad_params, lambdas);
    // Double quadratic objective vals
    torch::Tensor double_quad_min_obj_vals;
    if (prob_tensors.double_quadratic_probs_tensor.defined()) {
        double_quad_min_obj_vals = get_double_quadratic_min_obj_vals_for_params(prob_tensors.double_quadratic_probs_tensor,
                                                                                double_quad_params,
                                                                                prob_tensors.C.size(0));
    }
    else {
        double_quad_min_obj_vals = torch::zeros({prob_tensors.C.size(0)}, torch::dtype(torch::kDouble));
    }

    // Single quadratic objective vals
    torch::Tensor single_quad_min_obj_vals;
    if (prob_tensors.single_quadratic_probs_tensor.defined()) {
        single_quad_min_obj_vals =  get_single_quadratic_min_obj_vals_for_params(prob_tensors.single_quadratic_probs_tensor,
                                                                                 single_quad_params,
                                                                                 prob_tensors.C.size(0));
    }
    else {
        single_quad_min_obj_vals = torch::zeros({prob_tensors.C.size(0)}, torch::dtype(torch::kDouble));
    }

    // Linear objective vals
    torch::Tensor lin_min_obj_vals =  get_linear_min_obj_vals_for_params(effective_c, prob_tensors.bounds, prob_tensors.C.size(0));
    
    // Final collation
    torch::Tensor final_obj_vals = -1*lamb.clone();
    final_obj_vals = final_obj_vals + double_quad_min_obj_vals;
    final_obj_vals = final_obj_vals + single_quad_min_obj_vals;
    final_obj_vals = final_obj_vals + lin_min_obj_vals;

    return final_obj_vals;
}

torch::Tensor get_loss_for_params(
    struct ProblemTensors& prob_tensors,
    const torch::Tensor& single_quad_params,
    const torch::Tensor& double_quad_params,
    const torch::Tensor& lambdas
)
{
    // 
    // Get effective C and lambda b
    //
    auto [effective_c, lamb] = get_effective_C_and_lambda_b(prob_tensors.A, prob_tensors.b, prob_tensors.C,
                                                            prob_tensors.single_var_inds_tensor,
                                                            prob_tensors.double_var_inds_tensor,
                                                            single_quad_params, double_quad_params, lambdas);

    // Double quadratic loss
    torch::Tensor double_quadratic_loss;
    if (prob_tensors.double_quadratic_probs_tensor.defined()) {
        double_quadratic_loss =  get_double_quadratic_loss_vals_for_params(prob_tensors.double_quadratic_probs_tensor,
                                                                           double_quad_params,
                                                                           prob_tensors.C.size(0),
                                                                           BOUND_PENALTY_WEIGHT, OPT_WEIGHT);
    }
    else {
        double_quadratic_loss = torch::zeros({prob_tensors.C.size(0)}, torch::dtype(torch::kDouble));
    }

    // Single quadratic loss
    torch::Tensor single_quadratic_loss;
    if (prob_tensors.single_quadratic_probs_tensor.defined()) {
        single_quadratic_loss =  get_single_quadratic_loss_vals_for_params(prob_tensors.single_quadratic_probs_tensor,
                                                                           single_quad_params,
                                                                           prob_tensors.C.size(0),
                                                                           BOUND_PENALTY_WEIGHT, OPT_WEIGHT);
    }
    else {
        single_quadratic_loss = torch::zeros({prob_tensors.C.size(0)}, torch::dtype(torch::kDouble));
    }
    
    // Linear objective loss
    torch::Tensor lin_obj_loss = 
        get_linear_loss_vals_for_params(effective_c,
                                        prob_tensors.bounds,
                                        prob_tensors.C.size(0),
                                        BOUND_PENALTY_WEIGHT, OPT_WEIGHT);

    // Final collation
    torch::Tensor final_loss = double_quadratic_loss + single_quadratic_loss + lin_obj_loss;
    
    return final_loss;
}

QuadConstraintsSolution DualQuadConstraintsSolver::get_lower_bounds_for_combinations(const QuadConstraintsProblem& quad_constraints_problem)
{
    std::map<Utils::CoeffVarPairSet, double> final_constraints;

    //
    // Create variable number to variable index map
    //
    int ind = 0;
    std::map<int, int> var_num_to_ind_map;
    for (int var : quad_constraints_problem.variables) {
        var_num_to_ind_map[var] = ind;
        ind++;
    }

    //
    // Extract needed tensors from the specified problem
    //
    ProblemTensors prob_tensors = extract_problem_tensors(quad_constraints_problem, var_num_to_ind_map, false);

    torch::Tensor final_opt_vals;

    if (prob_tensors.trivially_unbounded_indices.size() != quad_constraints_problem.combinations_to_solve.size()) {
        //
        // This case is when there are problems to solve and not all of them are unbounded
        //

        //
        // Initialize the learning parameters (lambdas, S and D)
        //
        torch::Tensor single_quad_params;
        std::set<std::pair<int, int>> already_used_coefficients;

        if (prob_tensors.single_var_inds_tensor.defined()) {
            single_quad_params = torch::zeros({prob_tensors.single_var_inds_tensor.size(0), 1}, torch::dtype(torch::kDouble));

            if (this->quad_initialization) {
                // Initialize the single split parameters S using the values in C
                // if the flag is on
                for (int i = 0; i < prob_tensors.single_var_inds_tensor.size(0); ++i) {
                    auto row = prob_tensors.single_var_inds_tensor[i];

                    int comb_idx = row[0].item<int>();
                    int var_ind = row[1].item<int>();

                    single_quad_params[i][0] = prob_tensors.C[comb_idx][var_ind];
                    already_used_coefficients.insert({comb_idx, var_ind});
                }
            }

            single_quad_params.set_requires_grad(true);
        }

        torch::Tensor double_quad_params;
        if (prob_tensors.double_var_inds_tensor.defined()) {
            double_quad_params = torch::zeros({prob_tensors.double_var_inds_tensor.size(0), 2}, torch::dtype(torch::kDouble));

            if (this->quad_initialization) {
                // Initialize the double split parameters D using the values in C which are not already used
                // (only if the flag is on)
                for (int i = 0; i < prob_tensors.double_var_inds_tensor.size(0); ++i) {
                    auto row = prob_tensors.double_var_inds_tensor[i];

                    int comb_idx = row[0].item<int>();

                    int var_ind = row[1].item<int>();
                    if (already_used_coefficients.find({comb_idx, var_ind}) == already_used_coefficients.end()) {
                        double_quad_params[i][0] = prob_tensors.C[comb_idx][var_ind];
                        already_used_coefficients.insert({comb_idx, var_ind});
                    }

                    var_ind = row[2].item<int>();
                    if (already_used_coefficients.find({comb_idx, var_ind}) == already_used_coefficients.end()) {
                        double_quad_params[i][1] = prob_tensors.C[comb_idx][var_ind];
                        already_used_coefficients.insert({comb_idx, var_ind});
                    }
                }            
            }

            double_quad_params.set_requires_grad(true);
        }
        
        torch::Tensor lambdas;
        if (prob_tensors.A.size(0) != 0) {
            // Create lambda parameters only if we have relational constraints and then initialize them to zero
            lambdas = torch::zeros({prob_tensors.C.size(0), prob_tensors.A.size(0)}, torch::dtype(torch::kDouble));
            lambdas.set_requires_grad(true);
        }

        //
        // Compute the interval relaxation answer as the baseline
        //
        torch::Tensor intv_opt_vals, intm_opt_vals;
        intv_opt_vals = get_obj_vals_for_params(prob_tensors, single_quad_params, double_quad_params, lambdas);
        final_opt_vals = intv_opt_vals;

        //
        // Setting up params for learning
        //
        std::vector<torch::optim::OptimizerParamGroup> groups;

        if (lambdas.defined()) {
            groups.emplace_back(std::vector<torch::Tensor>{lambdas},
                                std::make_unique<torch::optim::AdamOptions>(this->dual_lams_lr));
        }
        
        if (single_quad_params.defined()) {
            groups.emplace_back(std::vector<torch::Tensor>{single_quad_params},
                                std::make_unique<torch::optim::AdamOptions>(this->split_lams_lr));
        }

        if (double_quad_params.defined()) {
            groups.emplace_back(std::vector<torch::Tensor>{double_quad_params},
                                std::make_unique<torch::optim::AdamOptions>(this->split_lams_lr));
        }

        torch::optim::Adam optimizer(groups);

        torch::Tensor loss, loss_sum, lambda_params_intm, single_params_intm, double_params_intm;
        for (int i = 0; i < this->num_epochs; i++) {
            //
            // Optimization Loop
            //

            // Compute the loss in forward pass
            loss = get_loss_for_params(prob_tensors, single_quad_params, double_quad_params, lambdas);
            loss_sum = loss.sum();

            // Zero out the old gradients, backprop and do the descent
            optimizer.zero_grad();
            loss_sum.backward();
            optimizer.step();

            {
                torch::NoGradGuard no_grad;

                if (lambdas.defined()) {
                    lambdas.clamp_(0);
                    lambda_params_intm = torch::round(lambdas);
                }
                
                if (single_quad_params.defined()) {
                    single_params_intm = torch::round(single_quad_params);
                }

                if (double_quad_params.defined()) {
                    double_params_intm = torch::round(double_quad_params);
                }

                // 1. Try opt vals with rounded parameters
                intm_opt_vals = get_obj_vals_for_params(prob_tensors, single_params_intm, double_params_intm, lambda_params_intm);
                final_opt_vals = torch::maximum(final_opt_vals, intm_opt_vals);                 
            }
        }

        // Add the constant part of the objectives to the final optimal values
        final_opt_vals = final_opt_vals + prob_tensors.cons;
    }

    // Create and return the solution object
    QuadConstraintsSolution solution;
    int all_idx = 0;
    int cmb_idx = 0;
    for (auto comb: quad_constraints_problem.combinations_to_solve) {
        if (prob_tensors.trivially_unbounded_indices.find(all_idx) != prob_tensors.trivially_unbounded_indices.end()) {
            // If index in the trivially unbounded set, set -INF
            final_constraints[comb] = -INF;
        }
        else {
            // If not unbounded, then use the appropriate comb index to retrieve this value
            final_constraints[comb] = get_dbl_val_from_tensor(final_opt_vals[cmb_idx]);
            cmb_idx += 1;
        }

        all_idx += 1;
    }

    solution.lower_bounds_map = final_constraints;

    return solution;
}

void DualQuadConstraintsSolver::set_params(const std::unordered_map<std::string, std::string>& parameters)
{
    for (const auto& pair : parameters) {
        const std::string& key = pair.first;
        const std::string& value = pair.second;
        std::string lower_val = value;
        std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);

        // Check if the key is one of the expected ones
        if (key == "dual_lams_lr") {
            this->dual_lams_lr = Utils::parse_string_to_double(value);
        } else if (key == "split_lams_lr") {
            this->split_lams_lr = Utils::parse_string_to_double(value);
        } else if (key == "num_epochs") {
            this->num_epochs = Utils::parse_string_to_double(value);
        } else if (key == "num_threads") {
            this->num_threads = Utils::parse_string_to_int(value);
        } else if (key == "quad_initialization") {
            this->quad_initialization = (lower_val == "true" || lower_val == "1");
        }
        else if (key == "use_gpu") {
            if (lower_val == "true" || lower_val == "1") {
                if (is_torch_cuda_available()) {
                    this->device = torch::kCUDA;
                }
                else {
                    this->device = torch::kCPU;
                    std::cout << "WARNING: use_gpu set to true but cuda not available so using cpu instead!" << std::endl;
                }
            }
            else {
                this->device = torch::kCPU;
            }
        } else {
            // Throw error if an unexpected key is found
            throw std::invalid_argument("Unexpected key: " + key);
        }
    }

    torch::manual_seed(42);  // Set seed for reproducibility
    torch::set_num_threads(this->num_threads);
};
