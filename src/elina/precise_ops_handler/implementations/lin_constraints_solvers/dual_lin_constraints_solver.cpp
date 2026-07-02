#include "dual_lin_constraints_solver.hpp"
#include "utils.hpp"

#include <algorithm>
#include <dlfcn.h>
#include <limits>
#include <iostream>
#include <map>
#include <tuple>

//
// Constants
//
const double INF = std::numeric_limits<double>::infinity();
const int BOUND_PENALTY_WEIGHT = 1000;
const int OPT_WEIGHT = 100;
const int MASK_LR_BUMP_FACTOR = 5;

struct ProblemTensors {
    //
    // Let num_rel_constraints be the number of constraints that are not just bound on some variables
    //
    torch::Tensor A;  // Shape: [num_rel_constraints, num_vars]
    torch::Tensor b;  // Shape: [num_rel_constraints]
    torch::Tensor bounds; // Shape: [num_vars, 2]
    torch::Tensor C;  // Shape: [num_combs, num_vars]
    torch::Tensor ck; // Shape: [num_combs]
    torch::Tensor adaptive_learning_mask; // Shape (same as lambdas): [num_combs, num_vars] (Only generated if "use_adaptive_learning_mask" parameter is set to True)
};

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

ProblemTensors extract_problem_tensors(const LinConstraintsProblem& lin_constraints_problem, std::map<int, int> var_num_to_ind_map, bool parse_adaptive_learning_mask)
{
    ProblemTensors tensors;

    //
    // Parse all constraints in A and b
    //
    tensors.A = torch::zeros({lin_constraints_problem.constraints_map.size(), lin_constraints_problem.variables.size()}, torch::dtype(torch::kDouble));
    tensors.b = torch::zeros({lin_constraints_problem.constraints_map.size()}, torch::dtype(torch::kDouble));
    int var, var_ind, constraint_idx;
    double b_val, coeff;
    constraint_idx = 0;
    for (const auto& entry : lin_constraints_problem.constraints_map) {
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
    for (const auto& [var, bounds] : lin_constraints_problem.var_bounds_map) {
        double lb = bounds.first;
        double ub = bounds.second;
        var_ind = var_num_to_ind_map[var];

        tensors.bounds.index_put_({var_ind, 0}, lb);
        tensors.bounds.index_put_({var_ind, 1}, ub);
    }

    //
    // Parse combinations in C and ck
    //
    tensors.C = torch::zeros({lin_constraints_problem.combinations_to_solve.size(), lin_constraints_problem.variables.size()}, torch::dtype(torch::kDouble));
    tensors.ck = torch::zeros({lin_constraints_problem.combinations_to_solve.size()}, torch::dtype(torch::kDouble));
    int comb_idx = 0;
    for (const auto& combination : lin_constraints_problem.combinations_to_solve) {
        for (const auto& coeff_var_pair : combination) {
            coeff = coeff_var_pair.coeff;
            var = coeff_var_pair.var;
            var_ind = var_num_to_ind_map[var];

            if (var == -1) {
                tensors.ck.index_put_({comb_idx}, tensors.ck.index({comb_idx}) + coeff);
            }
            else{
                tensors.C.index_put_({comb_idx, var_ind}, tensors.C.index({comb_idx, var_ind}) + coeff);
            }
        }
        comb_idx += 1;
    }

    if (parse_adaptive_learning_mask) {
        //
        // Generate the adaptive learning mask
        //
        torch::Tensor A_mask = tensors.A.ne(0);
        torch::Tensor C_mask = tensors.C.ne(0);

        const int64_t k = C_mask.size(0);
        std::vector<torch::Tensor> masks;

        for (int64_t i = 0; i < k; ++i) {
            auto c_mask = C_mask[i].unsqueeze(0).expand({A_mask.size(0), -1});
            auto valid = (A_mask | (~c_mask)).all(1); // [m] — A_j covers all vars in c_i
            masks.push_back(valid);
        }

        tensors.adaptive_learning_mask = torch::stack(masks); 
    }

    return tensors;
}

torch::Tensor get_optimal_value_for_lambdas(
    torch::Tensor A, // Shape: [num_rel_constraints, num_vars]
    torch::Tensor b, // Shape: [num_rel_constraints]
    torch::Tensor bounds, // Shape: [num_vars, 2]
    torch::Tensor C, // Shape: [num_combs, num_vars]
    torch::Tensor lambdas // Shape: [num_combs, num_rel_constraints]
)
{
    //
    // We are solving max_{lam >= 0} min_{V} C * V + lam (A*V - b)
    // In this method, we need to find the inner min for given lambdas
    //

    //
    // Compute (lambda*A + C) and (lambda*b)
    //
    torch::Tensor lamA_plus_C;
    
    if (A.size(0) != 0) {
        lamA_plus_C = torch::matmul(lambdas, A) + C;
    }
    else {
        lamA_plus_C = C;
    }

    torch::Tensor lamb = torch::matmul(lambdas, b);

    //
    // Get the lower and upper variable bounds and broadcast them to match lamA_plus_C: [num_combs, num_vars]
    //
    torch::Tensor lb = bounds.index({torch::indexing::Slice(), 0});
    torch::Tensor ub = bounds.index({torch::indexing::Slice(), 1});
    torch::Tensor lb_broadcast = lb.unsqueeze(0).expand_as(lamA_plus_C);
    torch::Tensor ub_broadcast = ub.unsqueeze(0).expand_as(lamA_plus_C);

    //
    // Pick the bounds to use based on the sign of lamA_plus_C
    // As we need to minimize: for pos coeffs, pick lower bound and
    // for neg coeffs, pick upper bound.
    //
    torch::Tensor pos_mask = lamA_plus_C > 0;
    torch::Tensor picked_bounds = torch::where(pos_mask, lb_broadcast, ub_broadcast);
    
    // Zero out values where lamA_plus_C == 0
    picked_bounds = torch::where(lamA_plus_C == 0, torch::zeros_like(picked_bounds), picked_bounds);

    //
    // Compute the final answer
    //
    torch::Tensor opt_val = (lamA_plus_C * picked_bounds).sum(1) - lamb;

    return opt_val;
}

torch::Tensor get_optim_loss_for_lambdas(
    torch::Tensor A, // Shape: [num_rel_constraints, num_vars]
    torch::Tensor b, // Shape: [num_rel_constraints]
    torch::Tensor bounds, // Shape: [num_vars, 2]
    torch::Tensor C, // Shape: [num_combs, num_vars]
    torch::Tensor lambdas, // Shape: [num_combs, num_rel_constraints]
    int bound_penalty_weight, int opt_weight
)
{
    //
    // We are solving max_{lam >= 0} min_{V} C * V + lam (A*V - b)
    // In this method, we need to find the loss at given lambdas
    // 1. If the objective values can be found, then the loss is negative of that
    // as we need to find the max lower bounds
    // 2. For the cases where objective value is -infinite because of the picked bounds,
    // have a loss that will lead the solver to fixing the "problematic" coefficients
    //

    //
    // Compute (lambda*A + C) and (lambda*b)
    //
    torch::Tensor lamA_plus_C;
    
    if (A.size(0) != 0) {
        lamA_plus_C = torch::matmul(lambdas, A) + C;
    }
    else {
        lamA_plus_C = C;
    }

    torch::Tensor lamb = torch::matmul(lambdas, b);

    //
    // Get the lower and upper variable bounds and broadcast them to match lamA_plus_C: [num_combs, num_vars]
    //
    torch::Tensor lb = bounds.index({torch::indexing::Slice(), 0});
    torch::Tensor ub = bounds.index({torch::indexing::Slice(), 1});
    torch::Tensor lb_broadcast = lb.unsqueeze(0).expand_as(lamA_plus_C);
    torch::Tensor ub_broadcast = ub.unsqueeze(0).expand_as(lamA_plus_C);

    //
    // Pick the bounds to use based on the sign of lamA_plus_C
    //
    torch::Tensor pos_mask = lamA_plus_C > 0;
    torch::Tensor zero_mask = lamA_plus_C == 0;

    //
    // Pick the bounds to use based on the sign of lamA_plus_C
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
    torch::Tensor opt_val = (lamA_plus_C * safe_picked_bounds).sum(1) - lamb;

    //
    // Loss computation for the error cases
    //
    torch::Tensor inf_bound_picked_cases_mask = torch::isinf(picked_bounds);
    torch::Tensor inf_bound_picked_combs_mask = inf_bound_picked_cases_mask.any(1);

    torch::Tensor lamA_plus_C_sq = torch::pow(lamA_plus_C, 2);
    torch::Tensor inf_bound_loss = (lamA_plus_C_sq * inf_bound_picked_cases_mask.to(lamA_plus_C_sq.scalar_type())).sum(1);

    //
    // Final Loss: For cases with inf bound multiplication, use the inf bound loss, else use -1 * opt_val to get the best 
    // (highest lower) bound
    //
    torch::Tensor final_loss = torch::where(inf_bound_picked_combs_mask,
                                            bound_penalty_weight * inf_bound_loss,
                                            -1 * opt_weight * opt_val);

    return final_loss;
}

LinConstraintsSolution DualLinConstraintsSolver::get_lower_bounds_for_combinations(const LinConstraintsProblem& lin_constraints_problem)
{
    if (lin_constraints_problem.combinations_to_solve.size() == 0) {
        //
        // If there are no combinations to solve, return empty answer!
        //
        LinConstraintsSolution solution;
        return solution;
    }

    //
    // Create variable number to variable index map
    //
    int ind = 0;
    std::map<int, int> var_num_to_ind_map;
    for (int var :lin_constraints_problem.variables) {
        var_num_to_ind_map[var] = ind;
        ind++;
    }

    //
    // Extract needed tensors from the specified problem
    //
    ProblemTensors prob_tensors = extract_problem_tensors(lin_constraints_problem, var_num_to_ind_map, this->num_adaptive_learning_epochs > 0);

    //
    // Get the interval analysis answer (when lambdas = 0)
    //
    torch::Tensor final_opt_vals, intv_opt_vals, intm_opt_vals;
    intv_opt_vals = get_optimal_value_for_lambdas(prob_tensors.A, prob_tensors.b, prob_tensors.bounds, prob_tensors.C, torch::zeros({prob_tensors.C.size(0), prob_tensors.A.size(0)}, torch::dtype(torch::kDouble)));
    final_opt_vals = intv_opt_vals;

    if (prob_tensors.A.size(0) != 0) {
        //
        // If there are some linear relations apart from bounds, then perform gradient descent to find better
        // objective values than the interval analysis 
        //
        torch::Tensor lambdas = torch::zeros({prob_tensors.C.size(0), prob_tensors.A.size(0)}, torch::dtype(torch::kDouble));

        lambdas.set_requires_grad(true);
        double original_lr = this->learning_rate;
        torch::optim::Adam optimizer({lambdas}, torch::optim::AdamOptions(original_lr));
        torch::Tensor loss, loss_sum;

        for (int i = 0; i < this->num_epochs; i++) {
            //
            // Optimization Loop
            //

            // Compute the loss in forward pass
            loss = get_optim_loss_for_lambdas(prob_tensors.A, prob_tensors.b, prob_tensors.bounds, prob_tensors.C, lambdas, BOUND_PENALTY_WEIGHT, OPT_WEIGHT);
            loss_sum = loss.sum();

            // Zero out the old gradients, backprop and do the descent
            optimizer.zero_grad();
            loss_sum.backward();

            if (this->num_adaptive_learning_epochs > 0 && i < this->num_adaptive_learning_epochs) {
                if (i == 0) {
                    // Bump the LR for the mask learning phase
                    for (auto& g : optimizer.param_groups()) {
                        auto& opts = static_cast<torch::optim::AdamOptions&>(g.options());
                        opts.lr(original_lr * MASK_LR_BUMP_FACTOR);
                    }
                }

                // Clip the gradients when in the masked learning phase
                lambdas.grad().mul_(prob_tensors.adaptive_learning_mask);
            }

            optimizer.step();

            if (this->num_adaptive_learning_epochs > 0 && i == this->num_adaptive_learning_epochs - 1) {
                // Set the original LR when the mask learning phase is over
                for (auto& g : optimizer.param_groups()) {
                    auto& opts = static_cast<torch::optim::AdamOptions&>(g.options());
                    opts.lr(original_lr);
                }
            }

            {
                // In a no grad environment, first clip the lambdas and then update
                // the opt_vals.
                torch::NoGradGuard no_grad;
                lambdas.clamp_(0);
                                
                // 1. Try opt vals with rounded lambdas
                intm_opt_vals = get_optimal_value_for_lambdas(prob_tensors.A, prob_tensors.b, prob_tensors.bounds, prob_tensors.C, torch::round(lambdas));
                final_opt_vals = torch::maximum(final_opt_vals, intm_opt_vals);
            }
        }
    }

    //
    // Collate the final answer (add the ck constants to the optimized cTx) and return
    //
    final_opt_vals = final_opt_vals + prob_tensors.ck;
    std::map<Utils::CoeffVarSet, double> final_constraints;
    ind = 0;
    for (const auto& combination : lin_constraints_problem.combinations_to_solve) {
        final_constraints[combination] = get_dbl_val_from_tensor(final_opt_vals[ind]);
        ind += 1;
    }

    // Create and return the solution object
    LinConstraintsSolution solution;
    solution.lower_bounds_map = final_constraints;

    return solution;
}

void DualLinConstraintsSolver::set_params(const std::unordered_map<std::string, std::string>& parameters)
{
    for (const auto& pair : parameters) {
        const std::string& key = pair.first;
        const std::string& value = pair.second;
        std::string lower_val = value;
        std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);

        // Check if the key is one of the expected ones
        if (key == "learning_rate") {
            this->learning_rate = Utils::parse_string_to_double(value);
        } else if (key == "num_epochs") {
            this->num_epochs = Utils::parse_string_to_double(value);
        } else if (key == "num_adaptive_learning_epochs") {
            this->num_adaptive_learning_epochs = Utils::parse_string_to_double(value);
        } else if (key == "num_threads") {
            this->num_threads = Utils::parse_string_to_int(value);
        } else if (key == "use_gpu") {
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
