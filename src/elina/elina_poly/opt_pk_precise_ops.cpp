#include <assert.h>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "quad_affine_solver.hpp"
#include "utils.hpp"
#include "opt_pk_precise_ops.hpp"

std::vector<std::vector<int>> get_merged_var_components(int poly_dim)
{
    std::vector<std::vector<int>> var_components;
    std::vector<int> component;
    for(int i = 0; i < poly_dim; i++) {
        component.push_back(i);
    }
    var_components.push_back(component);

    return var_components;
};

elina_abstract0_t* opt_pk_quad_affine(elina_manager_t* man,
                                     elina_abstract0_t* a,
                                     QuadAffineBlock& quad_affine_block,
                                     bool split_problem_in_comps,
                                     std::unordered_map<std::string, std::string> linear_solver_config,
                                     std::unordered_map<std::string, std::string> quad_solver_config)
{
    if (quad_affine_block.size()!=0){
        //
        // Make the combined quad_affine_problem
        //
        QuadAffineProblem combined_quad_affine_problem;

        // Add the variables
        elina_dimension_t dimension = elina_abstract0_dimension(man, a);
        int poly_dim = dimension.intdim + dimension.realdim;
        for (int i = 0; i < poly_dim; i++) {
            combined_quad_affine_problem.variables.insert(i);
        }

        // Get the constraints, separate the variable bounds and set them in combined_quad_affine_problem 
        elina_lincons0_array_t cons_array = elina_abstract0_to_lincons_array(man, a);
        std::map<Utils::CoeffVarSet, double> constraints_map = Utils::parse_lincons_array_to_upper_bounds_map(&cons_array);
        combined_quad_affine_problem.var_bounds_map = Utils::extract_var_bounds_map_from_lincons_map(constraints_map);
        combined_quad_affine_problem.constraints_map = constraints_map;

        // Infer variable bounds from current constraints and update the bounds map
        // This is needed for polyhedra domain as some variable bounds could be hidden
        elina_interval_t** env = elina_abstract0_to_box(man, a);
        double lb, ub;
        double INF = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < poly_dim; ++i) {
            // Get lower bound and update in var bounds map if it is better than current
            elina_double_set_scalar(&lb,env[i]->inf,GMP_RNDD);
            if (lb != -INF) {
                combined_quad_affine_problem.var_bounds_map.try_emplace(i, -INF, INF);
                combined_quad_affine_problem.var_bounds_map[i].first = std::max(combined_quad_affine_problem.var_bounds_map[i].first, lb);
            }

            // Get upper bound and update in var bounds map if it is better than current
            elina_double_set_scalar(&ub,env[i]->sup,GMP_RNDU);
            if (ub != INF) {
                combined_quad_affine_problem.var_bounds_map.try_emplace(i, -INF, INF);
                combined_quad_affine_problem.var_bounds_map[i].second = std::min(combined_quad_affine_problem.var_bounds_map[i].second, ub);
            }
        }

        // Get the assignments from the assignment block
        combined_quad_affine_problem.assignments_map = quad_affine_block.get_assignments_map();

        // Set the template
        combined_quad_affine_problem.constraints_template_name = "oct";

        std::vector<QuadAffineProblem> quad_affine_problems;

        if(split_problem_in_comps) {
            //
            // Get the adjusted variable components and split the problems.
            // Polyhedra constraints don't expose a sparse component structure, so
            // all variables are treated as a single component.
            //
            std::vector<std::vector<int>> var_components = get_merged_var_components(poly_dim);
            quad_affine_problems = combined_quad_affine_problem.split_problem_in_components(var_components);
        }
        else {
            //
            // If splitting is disabled, solve the combined affine problem together
            //
            quad_affine_problems.push_back(combined_quad_affine_problem);
        }

        //
        // Once we have the problems ready, we can "forget" about the dimensions being assigned
        //
        std::vector<int> assigned_keys = quad_affine_block.get_assigned_keys();
        int numAssign = assigned_keys.size();
        elina_dim_t* tdim = new elina_dim_t[numAssign];
        for (size_t i = 0; i < numAssign; ++i) {
            tdim[i] = static_cast<elina_dim_t>(assigned_keys[i]);
        }
        elina_abstract0_forget_array(man, false, a, tdim, numAssign, false);

        //
        // Create the solver object and solve the quad affine problems
        // 
        QuadAffineSolver quad_affine_solver(linear_solver_config, quad_solver_config);
        std::vector<QuadAffineSolution> solver_answers;
    
        for (QuadAffineProblem quad_affine_prob: quad_affine_problems) {
            solver_answers.push_back(quad_affine_solver.solve_quad_affine(quad_affine_prob));
        }

        //
        // Use the solver answers to update the abstract element
        // 
        elina_lincons0_array_t solver_ans_cons_arr;
        for(QuadAffineSolution ans: solver_answers) {
            // Convert the solver answer to lincons array and take meet with the answer
            solver_ans_cons_arr = Utils::lower_bound_lincons_map_to_lincons_array(ans.new_constraints);
            if(solver_ans_cons_arr.size == 0) continue;
            a = elina_abstract0_meet_lincons_array(man, false, a, &solver_ans_cons_arr);
        }
    }

    return elina_abstract0_copy(man,a);
}