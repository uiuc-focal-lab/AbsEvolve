#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string> 

#include "utils.hpp"
#include "lin_constraints_solver.hpp"
#include "quad_constraints_solver.hpp"

const double INF = std::numeric_limits<double>::infinity();

void Utils::extract_from_cvp_set(const CoeffVarPairSet& cvp_set,
                                std::map<std::pair<int, int>, double>& pair_coeffs,
                                std::map<int, double>& var_coeffs,
                                double& k)
{
    // Decompose a CoeffVarPairSet into three canonical components:
    //   pair_coeffs : coefficients of quadratic terms  (c * x_i * x_j, i < j)
    //   var_coeffs  : coefficients of linear terms     (c * x_i)
    //   k           : constant term
    int v1, v2;
    double val;

    for (const auto& coeff_var_pair : cvp_set) {
        val = coeff_var_pair.first.coeff * coeff_var_pair.second.coeff;
        if (val == 0) continue;
        if (coeff_var_pair.first.var != -1 && coeff_var_pair.second.var != -1) {
            if (coeff_var_pair.first.var < coeff_var_pair.second.var) {
                v1 = coeff_var_pair.first.var; v2 = coeff_var_pair.second.var;
            }
            else {
                v1 = coeff_var_pair.second.var;  v2 = coeff_var_pair.first.var;
            }

            pair_coeffs[{v1, v2}] += val;
        }
        else if (coeff_var_pair.first.var != -1) {
            var_coeffs[coeff_var_pair.first.var] += val;
        }
        else if (coeff_var_pair.second.var != -1) {
            var_coeffs[coeff_var_pair.second.var] += val;
        }
        else if (coeff_var_pair.first.var == -1 && coeff_var_pair.second.var == -1) {
            k += val;
        }
    }
}

Utils::CoeffVarPairSet Utils::generate_cvp(std::map<std::pair<int, int>, double> pair_coeffs, std::map<int, double> var_coeffs, double k)
{
    // Collect the final results
    Utils::CoeffVarPairSet result;

    // Pair coeffs
    for (const auto& [key, value] : pair_coeffs) {
        int i = key.first;
        int j = key.second;
        double coeff = value;
        if (coeff == 0) continue;
        result.insert({Utils::CoeffVar(coeff, i), Utils::CoeffVar(1, j)});
    }

    // Single coeffs
    for (const auto& [key, value] : var_coeffs) {
        double coeff = value;
        if (coeff == 0) continue;
        result.insert({Utils::CoeffVar(coeff, key), Utils::CoeffVar(1, -1)});
    }

    // Constant
    if (k != 0) {
        result.insert({Utils::CoeffVar(k, -1), Utils::CoeffVar(1, -1)});
    }

    return result;
}

Utils::CoeffVarPairSet Utils::add_cvp_sets(Utils::CoeffVarPairSet ps1, Utils::CoeffVarPairSet ps2)
{
    std::map<std::pair<int, int>, double> pair_coeffs1, pair_coeffs2, pair_coeffs_final;
    std::map<int, double> var_coeffs1, var_coeffs2, var_coeffs_final;
    double k1 = 0, k2 = 0, kfinal = 0;

    // Parse the first set
    Utils::extract_from_cvp_set(ps1, pair_coeffs1, var_coeffs1, k1);

    // Parse the second set
    Utils::extract_from_cvp_set(ps2, pair_coeffs2, var_coeffs2, k2);

    // Add the results
    pair_coeffs_final = pair_coeffs1;
    for (const auto& [key, value] : pair_coeffs2) {
        pair_coeffs_final[key] += value;
    }

    var_coeffs_final = var_coeffs1;
    for (const auto& [key, value] : var_coeffs2) {
        var_coeffs_final[key] += value;
    }

    kfinal = k1 + k2;

    // Collect the final results
    Utils::CoeffVarPairSet sum_cvp = generate_cvp(pair_coeffs_final, var_coeffs_final, kfinal);

    return sum_cvp;
}

std::pair<Utils::CoeffVarPairSet, bool> Utils::multiply_cvp_sets(Utils::CoeffVarPairSet cvp_set1, Utils::CoeffVarPairSet cvp_set2)
{
    std::map<std::pair<int,int>, double> pair1, pair2;
    std::map<int,double>                 var1 , var2;
    double k1 = 0.0, k2 = 0.0;
    bool is_result_quadratic = false;

    // Parse the two sets
    Utils::extract_from_cvp_set(cvp_set1, pair1, var1, k1);
    Utils::extract_from_cvp_set(cvp_set2, pair2, var2, k2);

    if ((!pair1.empty() && (!pair2.empty() || !var2.empty())) ||
        (!pair2.empty() && (!pair1.empty() || !var1.empty())))
    {
        // Throw an error if the specified sets are such that the product is
        // of degree more than quadratic
        throw std::runtime_error("multiply_cvp_sets: operands must be such that the product is never more than quadratic");
    }

    // Result containers 
    std::map<std::pair<int,int>, double> pair_final;
    std::map<int,double>                 var_final;
    double k_final = k1 * k2;

    // Quadratic terms  (a_i * b_j)

    if (!pair1.empty() && k2 != 0) {
        for (const auto& [pair, val] : pair1) {
            pair_final[pair] += val*k2;
            is_result_quadratic = true;
        }
    }

    if (!pair2.empty() && k1 != 0) {
        for (const auto& [pair, val] : pair2) {
            pair_final[pair] += val*k1;
            is_result_quadratic = true;
        }
    }

    for (const auto& [i, ai] : var1)
        for (const auto& [j, bj] : var2)
        {
            double coeff = ai * bj;
            auto key = std::minmax(i, j);           // keep (small,large)
            pair_final[key] += coeff;
            is_result_quadratic = true;
        }

    // Linear terms  (k₁·b_j  +  k₂·a_i) 
    for (const auto& [j, bj] : var2)
        var_final[j] += k1 * bj;
    for (const auto& [i, ai] : var1)
        var_final[i] += k2 * ai;

    // Generate the final cvp set
    return {Utils::generate_cvp(pair_final, var_final, k_final), is_result_quadratic};
}

Utils::CoeffVarPairSet Utils::mult_cvp_set_k(CoeffVarPairSet ps, double K)
{
    std::map<std::pair<int, int>, double> pair_coeffs1, pair_coeffs_final;
    std::map<int, double> var_coeffs1, var_coeffs_final;
    double k1 = 0, kfinal = 0;

    // Parse the set
    Utils::extract_from_cvp_set(ps, pair_coeffs1, var_coeffs1, k1);

    // Multiply the result by K
    pair_coeffs_final = pair_coeffs1;
    for (const auto& [key, value] : pair_coeffs_final) {
        pair_coeffs_final[key] *= K;
    }

    var_coeffs_final = var_coeffs1;
    for (const auto& [key, value] : var_coeffs_final) {
        var_coeffs_final[key] *= K;
    }

    kfinal = k1 * K;

    // Collect the final results
    Utils::CoeffVarPairSet scaled_cvp = generate_cvp(pair_coeffs_final, var_coeffs_final, kfinal);

    return scaled_cvp;
}

int Utils::parse_string_to_int(const std::string& str) {
    try {
        int value = std::stoi(str);  // Converts string to int
        return value;
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: Invalid argument for int: " << e.what() << std::endl;
    } catch (const std::out_of_range& e) {
        std::cerr << "Error: Out of range for int: " << e.what() << std::endl;
    }
}

double Utils::parse_string_to_double(const std::string& str) {
    try {
        double value = std::stod(str);  // Converts string to double
        return value;
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: Invalid argument for double: " << e.what() << std::endl;
    } catch (const std::out_of_range& e) {
        std::cerr << "Error: Out of range for double: " << e.what() << std::endl;
    }
}

double parse_elina_coeff(elina_coeff_t coeff)
{
    if(coeff.discr == ELINA_COEFF_SCALAR &&
        coeff.val.scalar->discr == ELINA_SCALAR_DOUBLE)
     {
        return coeff.val.scalar->val.dbl;
     }
     else if(coeff.discr == ELINA_COEFF_SCALAR &&
             coeff.val.scalar->discr == ELINA_SCALAR_MPQ)
     {
        return mpq_get_d(coeff.val.scalar->val.mpq);
     }
     else {
         // We assume coefficients to be scalar doubles or mpqs.
         // Will handle this if seen otherwise.
         fprintf(stderr, "Coeff should be scalar doubles or scalar mpqs!");
         abort();
     }
}

Utils::CoeffVarSet Utils::linearize(Utils::CoeffVarPairSet cvp_set)
{
    Utils::CoeffVarSet val_set;
    
    int var;
    double coeff;
    for (const auto& coeff_var_pair : cvp_set) {
        var = -1;
        coeff = 1;

        if (coeff_var_pair.first.var != -1 && coeff_var_pair.second.var != -1) {
            // The specified cvp_set should be "linearizable"
            assert(false);
        }

        // Set the var
        if (coeff_var_pair.first.var != -1) {
            var = coeff_var_pair.first.var;
        }
        else if (coeff_var_pair.second.var != -1) {
            var = coeff_var_pair.second.var;
        }

        // Set the coeff
        coeff = coeff_var_pair.first.coeff * coeff_var_pair.second.coeff;
        if (coeff == 0) continue;

        // Insert in the final set
        val_set.insert(Utils::CoeffVar(coeff, var));
    }

    return val_set;
}

Utils::CoeffVarSet Utils::parse_linexpr(elina_linexpr0_t* linexpr)
{
    Utils::CoeffVarSet coeff_vars_set;

    //
    // Var -> Coefficients
    //
    int dim;
    double coeff;
    if (linexpr->discr == ELINA_LINEXPR_SPARSE) {
        elina_linterm_t linterm;
        for (int j=0; j<linexpr->size; j++) {
            linterm = linexpr->p.linterm[j];
            dim = linterm.dim;
            coeff = parse_elina_coeff(linterm.coeff);
            if (coeff == 0) continue;
            coeff_vars_set.insert(Utils::CoeffVar(coeff, dim));
        }
    }
    else {
        for (int j=0; j<linexpr->size; j++) {
            coeff = parse_elina_coeff(linexpr->p.coeff[j]);
            if (coeff != 0) {
                coeff_vars_set.insert(Utils::CoeffVar(coeff, j));
            }
        }
    }

    //
    // Constant
    //
    double cst = parse_elina_coeff(linexpr->cst);
    if (cst != 0) {
        coeff_vars_set.insert(Utils::CoeffVar(cst, -1));
    }

    return coeff_vars_set;
}

elina_linexpr0_t* Utils::coeff_var_set_and_cst_to_elina_linexpr(Utils::CoeffVarSet coeff_var_set, double cst, int mult) {
    int expr_size = 0;
    for (auto coeff_var: coeff_var_set) {
        if (coeff_var.var != -1) {
            expr_size += 1;
        }
    }

    elina_linexpr0_t *linexpr = elina_linexpr0_alloc(ELINA_LINEXPR_SPARSE, expr_size);

    int index = 0;
    double expr_cst = cst;
    elina_linterm_t *linterm;
    for (auto coeff_var: coeff_var_set) {
        if (coeff_var.var != -1) {
            linterm = &linexpr->p.linterm[index];
            linterm->dim = coeff_var.var;
            elina_scalar_set_double(linterm->coeff.val.scalar, mult * coeff_var.coeff);
            index += 1;
        }
        else {
            expr_cst += coeff_var.coeff;
        }
    }

    elina_scalar_set_double(linexpr->cst.val.scalar, mult * expr_cst);
    return linexpr;
}

std::map<Utils::CoeffVarSet, double> Utils::parse_lincons_array_to_upper_bounds_map(elina_lincons0_array_t* lincons_array)
{
    std::map<Utils::CoeffVarSet, double> lincons_map;
    elina_linexpr0_t* cons_expr;
    elina_constyp_t cons_type;
    double cst;
    Utils::CoeffVarSet coeff_vars_set;

    for (int i=0; i<lincons_array->size; i++) {
        cons_expr = lincons_array->p[i].linexpr0;
        cons_type = lincons_array->p[i].constyp;

        //
        // Parse the expression and add the mappings based on the constraint_type
        //
        coeff_vars_set = parse_linexpr(cons_expr);

        if (cons_type == ELINA_CONS_SUPEQ) {
            // >= case
            Utils::CoeffVarSet modified_coeff_vars_set;

            for (const auto& coeff_var : coeff_vars_set) {
                modified_coeff_vars_set.insert(Utils::CoeffVar(-coeff_var.coeff, coeff_var.var)); // Multiply key (coeff) by -1
            }

            lincons_map[modified_coeff_vars_set] = 0;
        }
        else if (cons_type == ELINA_CONS_EQ) {
            // == case -> broken down into >= and <=
            lincons_map[coeff_vars_set] = 0;

            Utils::CoeffVarSet modified_coeff_vars_set;

            for (const auto& coeff_var : coeff_vars_set) {
                modified_coeff_vars_set.insert(Utils::CoeffVar(-coeff_var.coeff, coeff_var.var)); // Multiply key (coeff) by -1
            }
            
            lincons_map[modified_coeff_vars_set] = 0;
        }
        else {
            // We assume all expressions to be of type >= or ==.
            // Will handle this if see otherwise.
            fprintf(stderr, "Constraint which is not >= or == type.");
            abort();
        }
    }

    return lincons_map;
}

std::map<int, std::pair<double, double>> Utils::extract_var_bounds_map_from_lincons_map(std::map<Utils::CoeffVarSet, double>& lincons_map)
{
    std::map<int, std::pair<double, double>> var_bounds_map;
    std::set<Utils::CoeffVarSet> keys_to_remove;
    
    //
    // Extract the variable bounds
    //
    int var;
    double coeff, bound;
    for(const auto [coeff_var_set, cons]: lincons_map) {
        if (coeff_var_set.size() > 2) continue; // Definitely not a variable bounds constraint

        if (coeff_var_set.size() == 1) {
            Utils::CoeffVar coeff_var1 = *coeff_var_set.begin();

            var = coeff_var1.var;
            coeff = coeff_var1.coeff;
            bound = (cons / coeff);
        }
        else {
            auto it = coeff_var_set.begin();
            Utils::CoeffVar coeff_var1 = *it;
            ++it;
            Utils::CoeffVar coeff_var2 = *it;

            if (coeff_var1.var != -1 && coeff_var2.var != -1) continue; // Not a variable bounds constraints

            if (coeff_var1.var != -1) {
                var = coeff_var1.var;
                coeff = coeff_var1.coeff;
                bound = (cons - coeff_var2.coeff) / (coeff);
            }
            else {
                var = coeff_var2.var;
                coeff = coeff_var2.coeff;
                bound = (cons - coeff_var1.coeff) / (coeff);
            }
        }

        // Set the bound
        var_bounds_map.try_emplace(var, -INF, INF);
        if (coeff > 0) {
            var_bounds_map[var].second = bound;
        }
        else {
            var_bounds_map[var].first = bound;
        }

        // Add the key for removal
        keys_to_remove.insert(coeff_var_set);
    }

    //
    // Remove the variable bounds from the specifed constraints map
    //
    for (auto key : keys_to_remove) {
        lincons_map.erase(key);
    }

    return var_bounds_map;
}

elina_lincons0_array_t Utils::lower_bound_lincons_map_to_lincons_array(std::map<Utils::CoeffVarSet, double>& lincons_map)
{
    // Filter out the -inf valued combinations
    std::vector<CoeffVarSet> keys_to_remove;
    for (const auto& [key, val] : lincons_map) {
        if (val == -INF) {
            keys_to_remove.push_back(key);
        }
    }

    for (auto k: keys_to_remove) {
        lincons_map.erase(k);
    }

    // The map stores lower bounds of the form: comb >= val.
    // Elina represents this as SUPEQ: -comb + val >= 0,
    // so coefficients are negated and the constant is set to -val.
    elina_lincons0_array_t array = elina_lincons0_array_make(lincons_map.size());
    elina_linexpr0_t* coeff_expr;
    int idx = 0;
    for(const auto [coeff_var_set, cons]: lincons_map) {
        coeff_expr = Utils::coeff_var_set_and_cst_to_elina_linexpr(coeff_var_set, -1*cons);
        array.p[idx] = elina_lincons0_make(ELINA_CONS_SUPEQ, coeff_expr, nullptr);
        idx += 1;
    }

    return array;
}

void Utils::print_vars_set(const std::set<int>& variables)
{
    std::cout << "[ ";
    for (auto it = variables.begin(); it != variables.end(); ++it) {
        if (it != variables.begin()) {
            std::cout << ", ";
        }
        std::cout << *it;
    }
    std::cout << " ]" << std::endl;
}

void Utils::print_var_bounds_map(const std::map<int, std::pair<double, double>>& var_bounds_map)
{
    for (const auto& [var, bounds] : var_bounds_map) {
        double lb = bounds.first;
        double ub = bounds.second;

        std::cout << "var " << var << ": [";
        if (std::isinf(lb)) std::cout << (lb < 0 ? "-inf" : "inf");
        else std::cout << lb;

        std::cout << ", ";

        if (std::isinf(ub)) std::cout << (ub < 0 ? "-inf" : "inf");
        else std::cout << ub;

        std::cout << "]\n";
    }
}

void Utils::print_combination(const Utils::CoeffVarSet& combination, bool pretty_print)
{
    if (pretty_print) {
        for (auto it = combination.begin(); it != combination.end(); ++it) {
            if(it->var != -1) {
                std::cout << "(" << it->coeff << " * x_" << it->var << ")";
            }
            else {
                std::cout << it->coeff;
            }
            if (std::next(it) != combination.end()) {
                std::cout << " + ";
            }
        }
    }
    else {
        for (const auto& coeff_var : combination) {
            std::cout << "(" << coeff_var.coeff << ", " << coeff_var.var << ") ";
        }
    }
    std::cout << std::endl;
}

void Utils::print_combination(const Utils::CoeffVarPairSet& combination, bool pretty_print)
{
    if (pretty_print) {
        for (auto it = combination.begin(); it != combination.end(); ++it) {
            std::cout << "( ";
            if(it->first.var != -1) {
                std::cout << it->first.coeff << " * x_" << it->first.var;
            }
            else {
                std::cout << it->first.coeff;
            }
            std::cout << " * ";
            if(it->second.var != -1) {
                std::cout << it->second.coeff << " * x_" << it->second.var;
            }
            else {
                std::cout << it->second.coeff;
            }
            std::cout << ") ";
            if (std::next(it) != combination.end()) {
                std::cout << " + ";
            }
        }
    }
    else {
        for (const auto& coeff_var_pair : combination) {
            std::cout << "(" << coeff_var_pair.first.coeff << ", " << coeff_var_pair.first.var << " | " << coeff_var_pair.second.coeff << ", " << coeff_var_pair.second.var << ") ";
        }
    }
    std::cout << std::endl;
}

void Utils::print_combinations(const std::set<Utils::CoeffVarSet>& combinations, bool pretty_print)
{
    for (const auto& comb : combinations) {
        Utils::print_combination(comb, pretty_print);
    }
}

void Utils::print_combinations(const std::set<Utils::CoeffVarPairSet>& combinations, bool pretty_print)
{
    for (const auto& comb : combinations) {
        Utils::print_combination(comb, pretty_print);
    }
}

void Utils::print_constraints_map(const std::map<Utils::CoeffVarSet, double>& constraints_map)
{
    std::cout << "{ ";

    for (const auto& mapEntry : constraints_map) {
        const auto& keySet = mapEntry.first;
        double value = mapEntry.second;

        std::cout << "( ";
        for (const auto& coeff_var : keySet) {
            std::cout << "(" << coeff_var.coeff << ", " << coeff_var.var << "), ";
        }
        std::cout << ") : " << value << "," << std::endl;
    }

    std::cout << "}" << std::endl;
}

void Utils::print_constraints_map(const std::map<Utils::CoeffVarPairSet, double>& constraints_map)
{
    std::cout << "{ ";

    for (const auto& mapEntry : constraints_map) {
        const auto& keySet = mapEntry.first;
        double value = mapEntry.second;

        std::cout << "( ";
        for (const auto& coeff_var_pair : keySet) {
            std::cout << "({" << coeff_var_pair.first.coeff << ", " << coeff_var_pair.first.var << "}, {" << coeff_var_pair.second.coeff << ", " << coeff_var_pair.second.var << "}), ";
        }
        std::cout << ") : " << value << "," << std::endl;
    }

    std::cout << "}" << std::endl;
}

void Utils::print_assignments_map(const std::map<int, Utils::CoeffVarSet>& assignment_map, bool pretty_print)
{
    std::cout << "{\n";
    for (const auto& [key, value_set] : assignment_map) {
        std::cout << "  " << key << ": ";
        Utils::print_combination(value_set, pretty_print);
    }
    std::cout << "}\n";
}

void Utils::print_assignments_map(const std::map<int, Utils::CoeffVarPairSet>& assignment_map, bool pretty_print)
{
    std::cout << "{\n";
    for (const auto& [key, value_set] : assignment_map) {
        std::cout << "  " << key << ": ";
        Utils::print_combination(value_set, pretty_print);
    }
    std::cout << "}\n";
}

std::string Utils::run_command(const std::string& command)
{
    // Use popen to run the command and capture output
    std::array<char, 128> buffer;
    std::string result;

    // Combine stdout and stderr using `2>&1`
    std::string full_command = command + " 2>&1";

    // Open the command for reading
    FILE* pipe = popen(full_command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to run command: " + full_command);
    }

    // Read the command output
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    // Close the pipe and check for errors
    int returnCode = pclose(pipe);
    if (returnCode != 0) {
        std::ostringstream oss;
        oss << "Command failed with return code " << returnCode << ": " << full_command;
        throw std::runtime_error(oss.str());
    }

    return result;  
}

void Utils::delete_file(const std::string& file_path) {
    if (std::remove(file_path.c_str()) != 0) {
        throw std::runtime_error("Failed to delete file: " + file_path);
    }
}

boost::json::value Utils::int_set_to_json(const std::set<int>& int_set)
{
    boost::json::array arr;
    for (int v : int_set) {
        arr.emplace_back(v);
    }
    return arr;
}

boost::json::value Utils::var_bounds_map_to_json(const std::map<int, std::pair<double, double>>& var_bounds_map)
{
    boost::json::object result;

    for (const auto& [var, bounds] : var_bounds_map) {
        boost::json::array bounds_arr;

        if (std::isinf(bounds.first)) {
            bounds_arr.emplace_back(bounds.first < 0 ? "-Infinity" : "Infinity");
        } else {
            bounds_arr.emplace_back(bounds.first);
        }

        if (std::isinf(bounds.second)) {
            bounds_arr.emplace_back(bounds.second < 0 ? "-Infinity" : "Infinity");
        } else {
            bounds_arr.emplace_back(bounds.second);
        }

        result[std::to_string(var)] = bounds_arr;
    }

    return result;
}


boost::json::value Utils::cv_set_to_json(const CoeffVarSet& cv_set)
{
    boost::json::array cv_arr;
    for (const auto& cv : cv_set) {
        boost::json::array arr;
        arr.emplace_back(cv.coeff);
        arr.emplace_back(cv.var);
        cv_arr.emplace_back(arr);
    }
    return cv_arr;
}

boost::json::value Utils::cvp_set_to_json(const CoeffVarPairSet& cvp_set)
{
    boost::json::array arr;
    for (const auto& pair : cvp_set) {
        boost::json::array inner;

        inner.emplace_back(pair.first.coeff);
        inner.emplace_back(pair.first.var);

        inner.emplace_back(pair.second.coeff);
        inner.emplace_back(pair.second.var);

        arr.emplace_back(inner);
    }
    return arr;   
}

boost::json::value Utils::combinations_map_to_json(const std::map<Utils::CoeffVarSet, double>& comb_to_double_map)
{
    boost::json::array result;

    for (const auto& [cv_set, coeff] : comb_to_double_map) {
        boost::json::object entry;
        entry["cv_set"] = cv_set_to_json(cv_set);
        if (std::isinf(coeff)) {
            entry["val"] = coeff < 0 ? "-Infinity" : "Infinity";
        } else {
            entry["val"] = coeff;
        }
        result.push_back(std::move(entry));
    }

    return result;
}

boost::json::value Utils::combinations_map_to_json(const std::map<Utils::CoeffVarPairSet, double>& comb_to_double_map)
{
    boost::json::array result;

    for (const auto& [cv_set, coeff] : comb_to_double_map) {
        boost::json::object entry;
        entry["cvp_set"] = cvp_set_to_json(cv_set);
        if (std::isinf(coeff)) {
            entry["val"] = coeff < 0 ? "-Infinity" : "Infinity";
        } else {
            entry["val"] = coeff;
        }
        result.push_back(std::move(entry));
    }

    return result;
}

boost::json::value Utils::combinations_to_json(const std::set<Utils::CoeffVarSet>& combinations)
{
    boost::json::array result;

    for (const auto& cv_set : combinations) {
        result.push_back(cv_set_to_json(cv_set));
    }

    return result;
}

boost::json::value Utils::combinations_to_json(const std::set<Utils::CoeffVarPairSet>& combinations)
{
    boost::json::array result;

    for (const auto& cvp_set : combinations) {
        result.push_back(cvp_set_to_json(cvp_set));
    }

    return result;
}

boost::json::value Utils::lin_cons_problem_to_json(const LinConstraintsProblem& lin_cons_problem)
{
    boost::json::object prob_json;

    prob_json["variables"] = Utils::int_set_to_json(lin_cons_problem.variables);
    prob_json["var_bounds"] = Utils::var_bounds_map_to_json(lin_cons_problem.var_bounds_map);
    prob_json["constraints"] = Utils::combinations_map_to_json(lin_cons_problem.constraints_map);
    prob_json["combinations"] = Utils::combinations_to_json(lin_cons_problem.combinations_to_solve);

    return prob_json;
}

boost::json::value Utils::quad_cons_problem_to_json(const QuadConstraintsProblem& quad_cons_problem)
{
    boost::json::object prob_json;

    prob_json["variables"] = Utils::int_set_to_json(quad_cons_problem.variables);
    prob_json["var_bounds"] = Utils::var_bounds_map_to_json(quad_cons_problem.var_bounds_map);
    prob_json["constraints"] = Utils::combinations_map_to_json(quad_cons_problem.constraints_map);
    prob_json["combinations"] = Utils::combinations_to_json(quad_cons_problem.combinations_to_solve);

    return prob_json;
}