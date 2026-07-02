
#include "quad_affine_solver.hpp"
#include "constraints_template.hpp"

std::vector<QuadAffineProblem> QuadAffineProblem::split_problem_in_components(std::vector<std::vector<int>> var_components)
{
    if (var_components.size() == 1) {
        // If there is just one component, no splitting is required
        return {*this};
    }

    // Create mapping from variables to component_idx
    std::map<int, int> var_to_comp_indx;
    for (int i = 0; i < var_components.size(); i++) {
        for (int var: var_components[i]) {
            var_to_comp_indx[var] = i;
        }
    }

    std::vector<QuadAffineProblem> quad_affine_problems(var_components.size());

    //
    // Split the variables
    //
    for (int i=0; i<var_components.size(); i++) {
        std::set<int> s(var_components[i].begin(), var_components[i].end());
        quad_affine_problems[i].variables = s;
    }

    //
    // Split the var bounds map
    //
    for (const auto& [var, bounds] : this->var_bounds_map) {
        quad_affine_problems[var_to_comp_indx.at(var)].var_bounds_map[var] = bounds;
    }

    //
    // Split the constraints
    //
    int comp_ind;
    for (const auto& [constr_comb, constr_value] : this->constraints_map) {
        comp_ind = -1;
        for (const auto&[coeff, var]: constr_comb) {
            if (var != -1) {
                comp_ind = var_to_comp_indx.at(var);
                break;
            }
        }
        quad_affine_problems[comp_ind].constraints_map[constr_comb] = constr_value;
    }

    //
    // Split the assignments
    //
    for (const auto& [var, assignment] : this->assignments_map) {
        quad_affine_problems[var_to_comp_indx.at(var)].assignments_map[var] = assignment;
    }

    //
    // Copy over the template name
    //
    for (int i=0; i<var_components.size(); i++) {
        quad_affine_problems[i].constraints_template_name = this->constraints_template_name;
    }
    
    //
    // Filter out problems with empty assignment maps
    //
    std::vector<QuadAffineProblem> filtered_quad_affine_problems;
    for (const auto& problem : quad_affine_problems) {
        if (!problem.assignments_map.empty()) {
            filtered_quad_affine_problems.push_back(problem);
        }
    }

    return filtered_quad_affine_problems;
}

bool extract_and_remove_log_flag(std::unordered_map<std::string, std::string>& config_map)
{
    auto it = config_map.find("log");
    if (it == config_map.end()) return false;

    std::string val = it->second;
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);

    config_map.erase(it);
    return (val == "true" || val == "1" || val == "yes");
}

QuadAffineSolver::QuadAffineSolver(const std::unordered_map<std::string, std::string>& lin_solver_config,
                                   const std::unordered_map<std::string, std::string>& quad_solver_config)
{
    m_lin_solver_config = lin_solver_config;
    m_quad_solver_config = quad_solver_config;

    m_log_linear_problems_as_json = extract_and_remove_log_flag(m_lin_solver_config);
    m_log_quadratic_problems_as_json = extract_and_remove_log_flag(m_quad_solver_config);
}

std::map<Utils::CoeffVarSet, Utils::CoeffVarPairSet> QuadAffineSolver::get_updated_combinations_after_affine(
    const std::set<Utils::CoeffVarSet>& combinations_to_compute,
    const std::map<int, Utils::CoeffVarPairSet>& assignment_map
)
{
    // For each combination in combinations_to_compute, substitute every variable
    // that has been assigned with its expression from assignment_map.
    // Returns a map from the original combination to its updated CoeffVarPairSet.
    std::map<Utils::CoeffVarSet, Utils::CoeffVarPairSet> updated_combs_map;

    for (const auto& combination : combinations_to_compute) {
        Utils::CoeffVarPairSet final_cvp_set;
        Utils::CoeffVarPairSet cvp_set_coeff;

        for (const auto& coeff_var : combination) {
            double coeff = coeff_var.coeff;
            int var = coeff_var.var;

            if (assignment_map.find(var) == assignment_map.end()) {
                cvp_set_coeff = {{Utils::CoeffVar(coeff, var), Utils::CoeffVar(1, -1)}};
            } else {
                cvp_set_coeff = assignment_map.at(var);
                cvp_set_coeff = Utils::mult_cvp_set_k(cvp_set_coeff, coeff);
            }

            if (final_cvp_set.empty()) {
                final_cvp_set = cvp_set_coeff;
            }
            else {
                final_cvp_set = Utils::add_cvp_sets(final_cvp_set, cvp_set_coeff);
            }
        }

        updated_combs_map[combination] = final_cvp_set;
    }

    return updated_combs_map;
}

std::pair<
    std::map<Utils::CoeffVarSet, Utils::CoeffVarSet>,
    std::map<Utils::CoeffVarSet, Utils::CoeffVarPairSet>
> split_into_linear_and_quadratic_combinations(std::map<Utils::CoeffVarSet, Utils::CoeffVarPairSet> updated_combinations_map)
{
    // A combination is linear if every term in its CoeffVarPairSet has at most one
    // variable (the other factor is a constant, i.e. var == -1).
    // Quadratic terms have two variables in the same pair.
    std::map<Utils::CoeffVarSet, Utils::CoeffVarSet> linear_combs_map;
    std::map<Utils::CoeffVarSet, Utils::CoeffVarPairSet> quadratic_combs_map;

    bool is_linear;
    for (const auto& kv : updated_combinations_map) {
        const Utils::CoeffVarPairSet& cvp_set = kv.second;

        is_linear = true;
        for (const auto& coeff_var_pair : cvp_set) {
            if (coeff_var_pair.first.var != -1 && coeff_var_pair.second.var != -1) {
                is_linear = false;
                break;
            }
        }

        if (is_linear) {
            linear_combs_map[kv.first] = Utils::linearize(cvp_set);
        }
        else {
            quadratic_combs_map[kv.first] = cvp_set;
        }
    }

    return {linear_combs_map, quadratic_combs_map};
}

LinConstraintsProblem generate_lincons_problem(const std::set<int>& all_variables,
                                               const std::map<int, std::pair<double, double>>& var_bounds_map,
                                               const std::map<Utils::CoeffVarSet, double>& constraints_map,
                                               const std::set<Utils::CoeffVarSet>& combinations_to_solve,
                                               bool optimize_generation)
{
    LinConstraintsProblem lin_cons_problem;

    // Set the specified constraints and variable bounds
    lin_cons_problem.var_bounds_map = var_bounds_map;
    lin_cons_problem.constraints_map = constraints_map;

    if (!optimize_generation) {
        // If the optimize_generation flag is off, do not optimize
        // by removing the redundant constraints
        lin_cons_problem.variables = all_variables;
        lin_cons_problem.combinations_to_solve = combinations_to_solve;

        return lin_cons_problem;
    }

    // Get the active variables (variables present in some of the constraints or for which we have bounds)
    // and set that in the lin_cons_problem
    std::set<int> active_variables;
    for (const auto& [constr_comb, _] : constraints_map) {
        for (const auto&[coeff, var]: constr_comb) {
            if (var != -1) {
                // Collect the variables that occur in the constraints
                active_variables.insert(var);
            }
        }
    }

    for (const auto& [var, _] : var_bounds_map) {
        active_variables.insert(var);
    }
    
    lin_cons_problem.variables = active_variables;

    // Add those combinations that have only the active variables
    bool to_include = true;
    for (auto& constr_comb : combinations_to_solve) {
        to_include = true;
        for (const auto&[_, var]: constr_comb) {
            if (var != -1 && !active_variables.count(var)) {
                to_include = false;
                break;
            }
        }

        if (to_include) lin_cons_problem.combinations_to_solve.insert(constr_comb);
    }

    return lin_cons_problem;
}

QuadConstraintsProblem generate_quadcons_problem(const std::set<int>& all_variables,
                                                 const std::map<int, std::pair<double, double>>& var_bounds_map,
                                                 const std::map<Utils::CoeffVarSet, double>& constraints_map,
                                                 const std::set<Utils::CoeffVarPairSet>& combinations_to_solve,
                                                 bool optimize_generation)
{
    QuadConstraintsProblem quad_cons_problem;

    // Set the specified constraints and variable bounds
    quad_cons_problem.var_bounds_map = var_bounds_map;
    quad_cons_problem.constraints_map = constraints_map;

    if (!optimize_generation) {
        // If the optimize_generation flag is off, do not optimize
        // by removing the redundant constraints
        quad_cons_problem.variables = all_variables;
        quad_cons_problem.combinations_to_solve = combinations_to_solve;

        return quad_cons_problem;
    }

    // Get the active variables (variables present in some of the constraints or for which we have bounds)
    // and set that in the quad_cons_problem
    std::set<int> active_variables;
    for (const auto& [constr_comb, _] : constraints_map) {
        for (const auto&[coeff, var]: constr_comb) {
            if (var != -1) {
                // Collect the variables that occur in the constraints
                active_variables.insert(var);
            }
        }
    }

    for (const auto& [var, _] : var_bounds_map) {
        active_variables.insert(var);
    }
    
    quad_cons_problem.variables = active_variables;

    // Add the combinations that have only the active variables
    bool to_include = true;
    for (auto& constr_comb : combinations_to_solve) {
        to_include = true;
        for (const auto&[coeff_var1, coeff_var2]: constr_comb) {
            if ((coeff_var1.var != -1 && !active_variables.count(coeff_var1.var)) ||
                (coeff_var2.var != -1 && !active_variables.count(coeff_var2.var)))
            {
                to_include = false;
                break;
            }
        }

        if (to_include) quad_cons_problem.combinations_to_solve.insert(constr_comb);
    }

    return quad_cons_problem;
}

QuadAffineSolution QuadAffineSolver::solve_quad_affine(const QuadAffineProblem& quad_affine_problem)
{
    auto start = std::chrono::high_resolution_clock::now();

    std::map<Utils::CoeffVarSet, double> new_constraints;

    //
    // Create the constraints_template and get the constraints to be computed
    //
    std::unique_ptr<ConstraintsTemplate> constraints_template = ConstraintsTemplate::create_template(quad_affine_problem.constraints_template_name);
    std::set<Utils::CoeffVarSet> combs_to_compute = constraints_template->get_combinations_to_compute(quad_affine_problem.variables, 
                                                                            Utils::get_map_keys(quad_affine_problem.assignments_map));

    //
    // Update the combinations to be computed based on the present assignment of variables
    //
    std::map<Utils::CoeffVarSet, Utils::CoeffVarPairSet> updated_vars_combs_after_affine =
        get_updated_combinations_after_affine(combs_to_compute, quad_affine_problem.assignments_map);

    //
    // Split the combinations into linear and quadratic
    //
    auto [linear_combs_map, quad_combs_map] = split_into_linear_and_quadratic_combinations(updated_vars_combs_after_affine);

    //
    // Handling the linear combinations
    //
    LinConstraintsProblem lin_cons_problem;
    LinConstraintsSolution lin_cons_sol;
    bool is_linear_non_empty = (linear_combs_map.size() != 0);
    double lin_solver_time = -1;

    if (is_linear_non_empty) {
        // Initialize the linear solver if it is not
        if (!m_lin_constraint_solver) {
            std::string solver_type = m_lin_solver_config.at("name");
            m_lin_solver_config.erase("name");
            m_lin_constraint_solver = LinConstraintsSolver::create_solver(solver_type, m_lin_solver_config);
        }

        //
        // Create the LinConstraintsProblem
        //
        lin_cons_problem = generate_lincons_problem(quad_affine_problem.variables,
                                                    quad_affine_problem.var_bounds_map,
                                                    quad_affine_problem.constraints_map,
                                                    Utils::get_map_values(linear_combs_map),
                                                    false);

        // Get the solution using the linear solver
        auto lin_solver_start_time = std::chrono::high_resolution_clock::now();
        lin_cons_sol = m_lin_constraint_solver->get_lower_bounds_for_combinations(lin_cons_problem);
        auto lin_solver_end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> lin_duration = lin_solver_end_time - lin_solver_start_time;
        lin_solver_time = lin_duration.count();

        // Use the solution to populate the new_constraints map
        for (const auto& [comb, effective_comb] : linear_combs_map) {
            auto it = lin_cons_sol.lower_bounds_map.find(effective_comb);
            double opt_val = (it != lin_cons_sol.lower_bounds_map.end()) ? it->second : -std::numeric_limits<double>::infinity();
            new_constraints[comb] = opt_val;
        }
    }

    //
    // Handling the quadratic combinations
    //
    QuadConstraintsProblem quad_cons_problem;
    QuadConstraintsSolution quad_cons_sol;
    bool is_quad_non_empty = (quad_combs_map.size() != 0);
    double quad_solver_time = -1;
    
    if (is_quad_non_empty) {
        // Initialize the quad solver if it is not
        if (!m_quad_constraint_solver) {
            std::string solver_type = m_quad_solver_config.at("name");
            m_quad_solver_config.erase("name");
            m_quad_constraint_solver = QuadConstraintsSolver::create_solver(solver_type, m_quad_solver_config);
        }

        //
        // Create the QuadConstraintsProblem
        //
        quad_cons_problem = generate_quadcons_problem(quad_affine_problem.variables,
                                                      quad_affine_problem.var_bounds_map,
                                                      quad_affine_problem.constraints_map,
                                                      Utils::get_map_values(quad_combs_map),
                                                      false);
        
        // Get the solution using the quad solver
        auto quad_solver_start_time = std::chrono::high_resolution_clock::now();
        quad_cons_sol = m_quad_constraint_solver->get_lower_bounds_for_combinations(quad_cons_problem);
        auto quad_solver_end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> quad_duration = quad_solver_end_time - quad_solver_start_time;
        quad_solver_time = quad_duration.count();

        // Use the solution to populate the new_constraints map
        for (const auto& [comb, effective_comb] : quad_combs_map) {
            auto it = quad_cons_sol.lower_bounds_map.find(effective_comb);
            double opt_val = (it != quad_cons_sol.lower_bounds_map.end()) ? it->second : -std::numeric_limits<double>::infinity();
            new_constraints[comb] = opt_val;
        }
    }

    // Record the end time
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;  // end to end duration in seconds
    // std::cout << "Time taken by quad_affine solver: " << duration.count() << " secs (" << lin_solver_time << ", " << quad_solver_time << " secs)" << std::endl;

    if (is_linear_non_empty && m_log_linear_problems_as_json) {
        boost::json::object prob_stats;
        prob_stats["type"] = "Linear";
        prob_stats["problem"] = Utils::lin_cons_problem_to_json(lin_cons_problem);
        prob_stats["solution"] = Utils::combinations_map_to_json(lin_cons_sol.lower_bounds_map);
        prob_stats["solver_time"] = lin_solver_time;
        prob_stats["total_time"] = duration.count();
        std::cout << boost::json::serialize(prob_stats) << std::endl;
    }
    
    if (is_quad_non_empty && m_log_quadratic_problems_as_json) {
        boost::json::object prob_stats;
        prob_stats["type"] = "Quadratic";
        prob_stats["problem"] = Utils::quad_cons_problem_to_json(quad_cons_problem);
        prob_stats["solution"] = Utils::combinations_map_to_json(quad_cons_sol.lower_bounds_map);
        prob_stats["solver_time"] = quad_solver_time;
        prob_stats["total_time"] = duration.count();
        std::cout << boost::json::serialize(prob_stats) << std::endl;
    }

    QuadAffineSolution solution;
    solution.new_constraints = new_constraints;

    return solution;
}