#include "gb_lin_constraints_solver.hpp"
#include <gurobi_c++.h>
#include <algorithm>
#include <iostream>
#include <limits>
#include <cassert>
#include <omp.h>
#include <string>

const double INF = std::numeric_limits<double>::infinity();

double round_down_if_not_integral(double val) {
    return (std::floor(val) == val) ? val : std::floor(val);
}

double get_min_obj_val_from_model(GRBModel& model, GRBLinExpr& obj) {
    model.setObjective(obj, GRB_MINIMIZE);
    double optVal = std::numeric_limits<double>::quiet_NaN();
    model.optimize();

    int status = model.get(GRB_IntAttr_Status);

    if (status == GRB_OPTIMAL) {
        optVal = model.get(GRB_DoubleAttr_ObjBound);
    } else if (status == GRB_INFEASIBLE) {
        optVal = INF;
    } else if (status == GRB_UNBOUNDED) {
        optVal = -INF;
    } else if (status == GRB_INF_OR_UNBD) {
        model.set(GRB_IntParam_Presolve, 0);
        model.optimize();

        status = model.get(GRB_IntAttr_Status);

        if (status == GRB_OPTIMAL) {
            optVal = model.get(GRB_DoubleAttr_ObjBound);
        } else if (status == GRB_INFEASIBLE) {
            optVal = INF;
        } else if (status == GRB_UNBOUNDED) {
            optVal = -INF;
        } else if (status == GRB_NUMERIC) {
            // We sometimes encounter this; so over-approximating to -inf for now.
            optVal = -INF;
        }
        else {
            std::cout << "Unhandled status after presolve disable: " << status << std::endl;
            assert(false);
        }
        model.set(GRB_IntParam_Presolve, -1);
    } else if (status == GRB_NUMERIC) {
        // We sometimes encounter this; so over-approximating to -inf for now.
        optVal = -INF;
    } else {
        std::cerr << "Unhandled Gurobi Status: " << status << std::endl;
        assert(false);
    }

    return optVal;
}

GRBLinExpr generate_term_from_constraint_list(const std::map<int, GRBVar>& variables_map, 
                                              const Utils::CoeffVarSet& cons_set)
{
    GRBLinExpr term = 0;

    for (const auto& coeff_var : cons_set) {
        if (coeff_var.var == -1) {
            term += coeff_var.coeff;
        } else {
            GRBVar gurobi_var = variables_map.at(coeff_var.var);
            term += coeff_var.coeff * gurobi_var;
        }
    }

    return term;
}

std::map<int, GRBVar> initialize_variables_map(GRBModel& model, const std::set<int>& variables, const std::map<int, std::pair<double, double>>& var_bounds_map)
{
    std::map<int, GRBVar> variables_map;
    int index = 0;
    double lb, ub;
    for (int var_id : variables) {
        lb = -GRB_INFINITY;
        ub = GRB_INFINITY;

        auto it = var_bounds_map.find(var_id);
        if (it != var_bounds_map.end()) {
            lb = it->second.first;
            ub = it->second.second;
        }

        variables_map[var_id] = model.addVar(
            lb, ub, 0.0, GRB_CONTINUOUS, "v_" + std::to_string(index));

        ++index;
    }

    model.update();
    return variables_map;
}

void add_constraints_to_model(
    GRBModel& model,
    const std::map<int, GRBVar>& variables_map,
    const std::map<Utils::CoeffVarSet, double>& constraints_map)
{
    for (const auto& pair : constraints_map) {
        GRBLinExpr term = generate_term_from_constraint_list(variables_map, pair.first);
        model.addConstr(term <= pair.second);
    }

    model.update(); // Update after adding all constraints
}

LinConstraintsSolution solve_combinations_sequential(
    const std::set<int>& variables,
    const std::map<int, std::pair<double, double>>& var_bounds_map,
    const std::map<Utils::CoeffVarSet, double>& constraints_map,
    const std::set<Utils::CoeffVarSet>& combinations_to_solve,
    int num_threads, int log_output_flag)
{
    std::map<Utils::CoeffVarSet, double> final_constraints;

    try {
        GRBEnv env = GRBEnv();
        env.set(GRB_IntParam_OutputFlag, log_output_flag);

        GRBModel model = GRBModel(env);
        model.set(GRB_IntParam_OutputFlag, log_output_flag);
        model.set(GRB_IntParam_Threads, num_threads); // Set number of threads that Gurobi can use for internal optimization.
        model.set(GRB_IntParam_NumericFocus, 3);
        model.set(GRB_DoubleParam_FeasibilityTol, 1e-9);
        model.set(GRB_DoubleParam_OptimalityTol, 1e-9);

        auto variables_map = initialize_variables_map(model, variables, var_bounds_map);
        add_constraints_to_model(model, variables_map, constraints_map);

        for (const auto& comb : combinations_to_solve) {
            GRBLinExpr term_comb = generate_term_from_constraint_list(variables_map, comb);
            double val = get_min_obj_val_from_model(model, term_comb);
            final_constraints[comb] = round_down_if_not_integral(val);
        }
    } catch (GRBException& e) {
        std::cout << "Error code = " << e.getErrorCode() << std::endl;
        std::cout << e.getMessage() << std::endl;
    } catch (...) {
        std::cout << "Exception during Gurobi optimization!" << std::endl;
    }

    // Create and return the solution object
    LinConstraintsSolution solution;
    solution.lower_bounds_map = final_constraints;

    return solution;
}

LinConstraintsSolution solve_combinations_parallel(
    const std::set<int>& variables,
    const std::map<int, std::pair<double, double>>& var_bounds_map,
    const std::map<Utils::CoeffVarSet, double>& constraints_map,
    const std::set<Utils::CoeffVarSet>& combinations_to_solve,
    int num_threads, int log_output_flag)
{
    std::map<Utils::CoeffVarSet, double> final_constraints;

    std::vector<Utils::CoeffVarSet> combinations_vec(combinations_to_solve.begin(), combinations_to_solve.end());
    std::vector<double> results(combinations_vec.size(), 0.0);

    omp_set_num_threads(num_threads);

    #pragma omp parallel
    {
        GRBEnv local_env = GRBEnv();
        local_env.set(GRB_IntParam_OutputFlag, log_output_flag);

        #pragma omp for
        for (size_t i = 0; i < combinations_vec.size(); ++i) {
            try {
                GRBModel model = GRBModel(local_env);
                model.set(GRB_IntParam_OutputFlag, log_output_flag);
                model.set(GRB_IntParam_Threads, 1); // Use 1 thread per model to avoid contention
                model.set(GRB_IntParam_NumericFocus, 3);
                model.set(GRB_DoubleParam_FeasibilityTol, 1e-9);
                model.set(GRB_DoubleParam_OptimalityTol, 1e-9);

                auto variables_map = initialize_variables_map(model, variables, var_bounds_map);
                add_constraints_to_model(model, variables_map, constraints_map);

                GRBLinExpr term_comb = generate_term_from_constraint_list(variables_map, combinations_vec[i]);
                results[i] = round_down_if_not_integral(get_min_obj_val_from_model(model, term_comb));
            } catch (GRBException& e) {
                std::cout << "Error code = " << e.getErrorCode() << std::endl;
                std::cout << e.getMessage() << std::endl;
            } catch (...) {
                std::cout << "Exception during Gurobi optimization!" << std::endl;
            }
        }
    }

    // Map results back to final_constraints
    for (size_t i = 0; i < combinations_vec.size(); ++i) {
        final_constraints[combinations_vec[i]] = results[i];
    }

    // Create and return the solution object
    LinConstraintsSolution solution;
    solution.lower_bounds_map = final_constraints;

    return solution;
}


LinConstraintsSolution GbLinConstraintsSolver::get_lower_bounds_for_combinations(const LinConstraintsProblem& lin_constraints_problem)
{
    if(this->use_parallel_mode) {
        return solve_combinations_parallel(lin_constraints_problem.variables, 
                                           lin_constraints_problem.var_bounds_map,
                                           lin_constraints_problem.constraints_map,
                                           lin_constraints_problem.combinations_to_solve,
                                           this->num_threads, this->log_output_flag);
    }
    else {
        return solve_combinations_sequential(lin_constraints_problem.variables,
                                           lin_constraints_problem.var_bounds_map,
                                             lin_constraints_problem.constraints_map,
                                             lin_constraints_problem.combinations_to_solve,
                                             this->num_threads, this->log_output_flag);
    }
}

void GbLinConstraintsSolver::set_params(const std::unordered_map<std::string, std::string>& parameters)
{
    for (const auto& pair : parameters) {
        const std::string& key = pair.first;
        const std::string& value = pair.second;

        std::string lower_val = value;
        std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);

        // Check if the key is one of the expected ones
        if (key == "use_parallel_mode") {
            this->use_parallel_mode = (lower_val == "true" || lower_val == "1");
        } else if (key == "num_threads") {
            this->num_threads = Utils::parse_string_to_int(value);
        } else if (key == "log_output_flag") {
            this->log_output_flag = Utils::parse_string_to_int(value);
        } else {
            // Throw error if an unexpected key is found
            throw std::invalid_argument("Unexpected key: " + key);
        }
    }
}