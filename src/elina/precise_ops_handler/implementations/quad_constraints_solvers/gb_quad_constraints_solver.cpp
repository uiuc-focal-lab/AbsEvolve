#include "gb_quad_constraints_solver.hpp"

#include <gurobi_c++.h>
#include <omp.h>
#include <iostream>
#include <limits>
#include <cassert>
#include <string>

const double INF = std::numeric_limits<double>::infinity();

double get_min_obj_val_from_model(GRBModel& model, const GRBLinExpr& obj) {
    model.setObjective(obj, GRB_MINIMIZE);
    double optVal = std::numeric_limits<double>::quiet_NaN();
    model.optimize();

    int status = model.get(GRB_IntAttr_Status);

    if (status == GRB_OPTIMAL || status == GRB_TIME_LIMIT) {
        try {
            // Get objBound if it's valid
            optVal = model.get(GRB_DoubleAttr_ObjBound);
        } catch (GRBException &e) {
            // objBound is not valid, fallback to objVal
            optVal = model.get(GRB_DoubleAttr_ObjVal);
        }

        if (optVal == GRB_INFINITY) {
            optVal = INF;
        }
        else if (optVal == -GRB_INFINITY) {
            optVal = -INF;
        }
    } else if (status == GRB_INFEASIBLE) {
        optVal = INF;
    } else if (status == GRB_UNBOUNDED) {
        optVal = -INF;
    } else if (status == GRB_INF_OR_UNBD) {
        // Can safely over-approximate to -infinity in case when it is either inf or unbounded
        optVal = -INF;
    } else {
        std::cerr << "Unhandled Gurobi Status: " << status << std::endl;
        assert(false);
    }

    return optVal;
}

GRBQuadExpr generate_obj_term_from_comb_list(
    const std::map<int, GRBVar>& variables_map,
    const Utils::CoeffVarPairSet& cons_set)
{
    GRBQuadExpr term = 0;

    for (const auto& pair : cons_set) {
        const Utils::CoeffVar& first_coeff_var = pair.first;
        const Utils::CoeffVar& second_coeff_var = pair.second;

        // Handle first CoeffVar
        GRBLinExpr first_expr = 0;
        if (first_coeff_var.var != -1) {
            auto gb_var = variables_map.at(first_coeff_var.var);
            first_expr = gb_var * first_coeff_var.coeff;
        } else {
            first_expr = first_coeff_var.coeff;
        }

        // Handle second CoeffVar
        GRBLinExpr second_expr = 0;
        if (second_coeff_var.var != -1) {
            auto gb_var = variables_map.at(second_coeff_var.var);
            second_expr = gb_var * second_coeff_var.coeff;
        } else {
            second_expr = second_coeff_var.coeff;
        }

        // Add quadratic term
        term += first_expr * second_expr;
    }

    return term;
}

GRBLinExpr generate_lin_term_from_constraint_list(const std::map<int, GRBVar>& variables_map, 
                                                  const Utils::CoeffVarSet& cons_set)
{
    GRBLinExpr term = 0;

    for (const auto& coeff_var : cons_set) {
        if (coeff_var.var == -1) {
            term += coeff_var.coeff;
        } else {
            term += coeff_var.coeff * variables_map.at(coeff_var.var);
        }
    }

    return term;
}

std::map<int, GRBVar> initialize_variables_map(GRBModel& model, const std::set<int>& variables)
{
    std::map<int, GRBVar> variables_map;
    int index = 0;
    for (auto it = variables.begin(); it != variables.end(); ++it) {
        int var_id = *it;
        variables_map[var_id] = model.addVar(-GRB_INFINITY,
                                            GRB_INFINITY,
                                            0.0,
                                            GRB_CONTINUOUS,
                                            "v_" + std::to_string(index));
        index += 1;
    }

    // Extra variable to hold the quadratic objective value.
    // The objective is expressed as: objVar == quad_expr, so Gurobi
    // minimizes a linear objective (objVar) over a quadratic equality.
    variables_map[index] = model.addVar(-GRB_INFINITY,
                                        GRB_INFINITY,
                                        0.0,
                                        GRB_CONTINUOUS,
                                        "obj");

    model.update();

    return variables_map;
}

void add_constraints_to_model(
    GRBModel& model,
    const std::map<int, GRBVar>& variables_map,
    const std::map<Utils::CoeffVarSet, double>& constraints_map)
{
    for (const auto& pair : constraints_map) {
        GRBLinExpr term = generate_lin_term_from_constraint_list(variables_map, pair.first);
        model.addConstr(term <= pair.second);
    }

    model.update();
}

QuadConstraintsSolution solve_combinations_sequential(
    const std::set<int>& variables,
    const std::map<Utils::CoeffVarSet, double>& constraints_map,
    const std::set<Utils::CoeffVarPairSet>& combinations_to_solve,
    int num_threads,
    double tl_per_comb,
    int log_output_flag)
{
    std::map<Utils::CoeffVarPairSet, double> final_constraints;

    try {
        GRBEnv env = GRBEnv();
        GRBModel model = GRBModel(env);
        model.set(GRB_IntParam_OutputFlag, log_output_flag);
        model.set(GRB_IntParam_Threads, num_threads); // Set number of threads that Gurobi can use for internal optimization.
        model.set(GRB_IntParam_NonConvex, 2);   // allow non-convex quadratic objectives
        model.set(GRB_DoubleParam_TimeLimit, tl_per_comb);
        model.set(GRB_IntParam_ScaleFlag, 2);   // aggressive scaling for numerical stability

        auto variables_map = initialize_variables_map(model, variables);
        GRBVar objVar = variables_map[variables.size()];
        add_constraints_to_model(model, variables_map, constraints_map);

        GRBQuadExpr obj_expr;
        GRBQConstr obj_constr;
        for (const auto& comb : combinations_to_solve) {
            obj_expr = generate_obj_term_from_comb_list(variables_map, comb);
            obj_constr = model.addQConstr(objVar == obj_expr);
            model.update();

            double val = get_min_obj_val_from_model(model, objVar);
            final_constraints[comb] = val;

            model.remove(obj_constr);
            model.update();
        }
    } catch (GRBException& e) {
        std::cout << "Error code = " << e.getErrorCode() << std::endl;
        std::cout << e.getMessage() << std::endl;
    } catch (...) {
        std::cout << "Exception during Gurobi optimization!" << std::endl;
    }

    // Create and return the solution object
    QuadConstraintsSolution solution;
    solution.lower_bounds_map = final_constraints;

    return solution;
}

QuadConstraintsSolution solve_combinations_parallel(
    const std::set<int>& variables,
    const std::map<Utils::CoeffVarSet, double>& constraints_map,
    const std::set<Utils::CoeffVarPairSet>& combinations_to_solve,
    int num_threads,
    double tl_per_comb,
    int log_output_flag)
{
    std::map<Utils::CoeffVarPairSet, double> final_constraints;

    std::vector<Utils::CoeffVarPairSet> combinations_vec(combinations_to_solve.begin(), combinations_to_solve.end());
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
                model.set(GRB_IntParam_NonConvex, 2);   // allow non-convex quadratic objectives
                model.set(GRB_DoubleParam_TimeLimit, tl_per_comb);
                model.set(GRB_IntParam_ScaleFlag, 2);   // aggressive scaling for numerical stability

                auto variables_map = initialize_variables_map(model, variables);
                GRBVar objVar = variables_map[variables.size()];
                add_constraints_to_model(model, variables_map, constraints_map);
                
                GRBQuadExpr obj_expr = generate_obj_term_from_comb_list(variables_map, combinations_vec[i]);
                GRBQConstr obj_constr = model.addQConstr(objVar == obj_expr);
                model.update();
                results[i] = get_min_obj_val_from_model(model, objVar);
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
    QuadConstraintsSolution solution;
    solution.lower_bounds_map = final_constraints;

    return solution;
}

QuadConstraintsSolution GbQuadConstraintsSolver::get_lower_bounds_for_combinations(const QuadConstraintsProblem& quad_constraints_problem)
{
    if(this->use_parallel_mode) {
        return solve_combinations_parallel(quad_constraints_problem.variables, quad_constraints_problem.constraints_map, quad_constraints_problem.combinations_to_solve, this->num_threads, this->tl_per_comb, this->log_output_flag);
    }
    else {
        return solve_combinations_sequential(quad_constraints_problem.variables, quad_constraints_problem.constraints_map, quad_constraints_problem.combinations_to_solve, this->num_threads, this->tl_per_comb, this->log_output_flag);
    }
}

void GbQuadConstraintsSolver::set_params(const std::unordered_map<std::string, std::string>& parameters)
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
        } else if (key == "tl_per_comb") {
            this->tl_per_comb = Utils::parse_string_to_double(value);
        } else if (key == "log_output_flag") {
            this->log_output_flag = Utils::parse_string_to_int(value);
        } else {
            // Throw error if an unexpected key is found
            throw std::invalid_argument("Unexpected key: " + key);
        }
    }
}