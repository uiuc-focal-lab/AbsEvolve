#include <assert.h>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "quad_affine_solver.hpp"
#include "utils.hpp"
#include "opt_oct_precise_ops.hpp"
#include "opt_oct_hmat.h"
#include "opt_oct_internal.h"

std::vector<std::vector<int>> get_merged_var_components(opt_oct_mat_t* oo,
                                                        int oct_dim,
                                                        QuadAffineProblem& quad_affine_problem)
{
    double oct_size = 2*oct_dim*(oct_dim+1);
    double sparsity = 1- ((double)(oo->nni)/oct_size);

    //
    // Compute sparsity and update the matrix accordingly
    //
    if(sparsity >= sparse_threshold){
        if(oo->is_dense){
            oo->is_dense = false;
            oo->acl = extract(oo->mat, oct_dim);
        }
    }
    else{
        if(!oo->is_dense){
            if(!oo->ti){
                oo->ti = true;
                convert_to_dense_mat(oo, oct_dim, false);
            }
            oo->is_dense = true;
            free_array_comp_list(oo->acl);
        }
    }

    std::vector<std::vector<int>> var_components;

    if (!oo->is_dense) {
        //
        // Matrix is sparse, so get the "possible" future components
        //
        
        // Copy the original component list.
        array_comp_list_t *copy_component_list_arr = copy_array_comp_list(oo->acl);

        // Based on the assignments, merge the list into "possible"
        // future components.
        for (const auto& pair : quad_affine_problem.assignments_map) {
            // Create this component.
    		comp_list_t *cl = create_comp_list();
            insert_comp(cl, pair.first);

            for (const auto& coeff_var_pair : pair.second) {
                if (coeff_var_pair.first.var != -1 && coeff_var_pair.first.var != pair.first) {
                    insert_comp(cl, coeff_var_pair.first.var);
                }

                if (coeff_var_pair.second.var != -1 && coeff_var_pair.second.var != pair.first) {
                    insert_comp(cl, coeff_var_pair.second.var);
                }
            }

            // Merge.
            insert_comp_list_with_union(copy_component_list_arr, cl, oct_dim);
        }

        comp_list_t* travOuter = copy_component_list_arr->head;
        while (travOuter != NULL) {
            std::vector<int> component;
            comp_t* travInner = travOuter->head;
            
            while (travInner != NULL) {
                component.push_back(travInner->num);
                travInner=travInner->next;
            }

            travOuter=travOuter->next;
            var_components.push_back(component);
        }
    }
    else {
        //
        // Matrix is dense
        //
        std::vector<int> component;
        for(int i = 0; i < oct_dim; i++) {
            component.push_back(i);
        }
        var_components.push_back(component);
    }

    return var_components;
};

elina_abstract0_t* opt_oct_quad_affine(elina_manager_t* man,
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
        int oct_dim = dimension.intdim + dimension.realdim;
        for (int i = 0; i < oct_dim; i++) {
            combined_quad_affine_problem.variables.insert(i);
        }

        // Get the constraints, separate the variable bounds and set them in combined_quad_affine_problem 
        elina_lincons0_array_t cons_array = elina_abstract0_to_lincons_array(man, a);
        std::map<Utils::CoeffVarSet, double> constraints_map = Utils::parse_lincons_array_to_upper_bounds_map(&cons_array);
        combined_quad_affine_problem.var_bounds_map = Utils::extract_var_bounds_map_from_lincons_map(constraints_map);
        combined_quad_affine_problem.constraints_map = constraints_map;

        // Get the assignments from the assignment block
        combined_quad_affine_problem.assignments_map = quad_affine_block.get_assignments_map();

        // Set the template
        combined_quad_affine_problem.constraints_template_name = "oct";

        std::vector<QuadAffineProblem> quad_affine_problems;
        if (split_problem_in_comps) {
            //
            // Get the adjusted variable components and split the problems
            //
            opt_oct_t* oct = (opt_oct_t*) a->value;
            opt_oct_mat_t* oct_mat = oct->closed ? oct->closed : oct->m;
            std::vector<std::vector<int>> var_components = get_merged_var_components(oct_mat, oct_dim, combined_quad_affine_problem);
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
    
        for (QuadAffineProblem quad_affine_problem: quad_affine_problems) {
            solver_answers.push_back(quad_affine_solver.solve_quad_affine(quad_affine_problem));
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