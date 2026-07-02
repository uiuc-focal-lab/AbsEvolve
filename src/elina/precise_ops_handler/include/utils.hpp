#ifndef UTILS_HPP
#define UTILS_HPP

#include <boost/json.hpp>
#include <map>
#include <set>
#include <stdbool.h>
#include <string>

#include "elina_abstract0.h"

// Forward declaration
class LinConstraintsProblem;
class QuadConstraintsProblem;

namespace json = boost::json;

class Utils {
public:
    // Structure to store a variable v and a coefficient c (denoting c*v)
    // v is integer index, if it is -1, then this represents constant c.
    struct CoeffVar {
        double coeff;
        int var;

        // Constructor for easier initialization
        CoeffVar(double c, int v) : coeff(c), var(v) {}

        // Define ordering for use in std::set
        bool operator<(const CoeffVar& other) const {
            return std::tie(coeff, var) < std::tie(other.coeff, other.var);
        }
    };

    using CoeffVarSet = std::set<CoeffVar>;
    using CoeffVarPair = std::pair<CoeffVar, CoeffVar>;
    using CoeffVarPairSet = std::set<CoeffVarPair>;

    // Static method to extract keys from a map and return them as a set
    template <typename K, typename V>
    static std::set<K> get_map_keys(const std::map<K, V>& input_map);

    // Static method to extract values from a map and return them as a set
    template <typename K, typename V>
    static std::set<V> get_map_values(const std::map<K, V>& input_map);

    // Static method to generate components from the specified CoeffVarPairSet
    static void extract_from_cvp_set(const CoeffVarPairSet& cvp_set,
                                     std::map<std::pair<int, int>, double>& pair_coeffs,
                                     std::map<int, double>& var_coeffs,
                                     double& k);

    // Static method to generate CoeffVarPairSet from the specified components
    static CoeffVarPairSet generate_cvp(std::map<std::pair<int, int>, double> pair_coeffs, std::map<int, double> var_coeffs, double k);

    // Static method to add two CoeffVarPairSets
    static CoeffVarPairSet add_cvp_sets(CoeffVarPairSet ps1, CoeffVarPairSet ps2);

    // Static method to multiply a CoeffVarPairSet by a constant
    static CoeffVarPairSet mult_cvp_set_k(CoeffVarPairSet ps, double K);

    // Static method to multiply two CoeffVarPairSets to get a CoeffVarPairSet
    static std::pair<CoeffVarPairSet, bool> multiply_cvp_sets(CoeffVarPairSet cvp_set1, CoeffVarPairSet cvp_set2);
    
    // Static method to linearize a CoeffVarPairSet
    static CoeffVarSet linearize(CoeffVarPairSet cvp_set);

    // Static function to parse elina linexpr
    static CoeffVarSet parse_linexpr(elina_linexpr0_t* linexpr);

    // Static function to convert CoeffVarSet to elina_linexpr0_t
    // mult is used to multiply the expression by the factor
    static elina_linexpr0_t* coeff_var_set_and_cst_to_elina_linexpr(Utils::CoeffVarSet coeff_var_set, double cst, int mult = 1);

    // Static function to parse elina_lincons0_array_t into a map from coeff_var_set to doubles
    // which represent equations of the form a*x + b*y + .. <= k
    static std::map<Utils::CoeffVarSet, double> parse_lincons_array_to_upper_bounds_map(elina_lincons0_array_t* lincons_array);

    // Static function to extract variable bounds present in the specified lincons_map
    static std::map<int, std::pair<double, double>> extract_var_bounds_map_from_lincons_map(std::map<Utils::CoeffVarSet, double>& lincons_map);

    // Static function to parse a lower bounds map (coeff_var_set to doubles) to elina_lincons0_array_t
    static elina_lincons0_array_t lower_bound_lincons_map_to_lincons_array(std::map<Utils::CoeffVarSet, double>& lincons_map);

    // Static function to parse string as int
    static int parse_string_to_int(const std::string& str);

    // Static function to parse string as double
    static double parse_string_to_double(const std::string& str);

    //
    // Helper functions to pretty print data structures we use.
    //

    // Static function to print a set of combinations
    static void print_vars_set(const std::set<int>& variables);

    // Static function to print var_bounds_map
    static void print_var_bounds_map(const std::map<int, std::pair<double, double>>& var_bounds_map);

    // Static function to print a combination
    static void print_combination(const CoeffVarSet& combination, bool pretty_print = true);
    static void print_combination(const CoeffVarPairSet& combination, bool pretty_print = true);
    
    // Static function to print a set of combinations
    static void print_combinations(const std::set<CoeffVarSet>& combinations, bool pretty_print = true);
    static void print_combinations(const std::set<CoeffVarPairSet>& combinations, bool pretty_print = true);

    // Static function to print a map of constraints
    static void print_constraints_map(const std::map<CoeffVarSet, double>& constraints_map);
    static void print_constraints_map(const std::map<CoeffVarPairSet, double>& constraints_map);

    // Static function to print a map of assignments
    static void print_assignments_map(const std::map<int, CoeffVarSet>& assignment_map, bool pretty_print = true);
    static void print_assignments_map(const std::map<int, CoeffVarPairSet>& assignment_map, bool pretty_print = true);

    // Static function to run a command and return its output.
    static std::string run_command(const std::string& command);

    // Static function to delete the specified file.
    static void delete_file(const std::string& file_path);

    //
    // Helper functions to convert various data structures to JSON
    //
    static boost::json::value int_set_to_json(const std::set<int>& int_set);
    static boost::json::value var_bounds_map_to_json(const std::map<int, std::pair<double, double>>& var_bounds_map);
    static boost::json::value cv_set_to_json(const CoeffVarSet& cv_set);
    static boost::json::value cvp_set_to_json(const CoeffVarPairSet& cvp_set);
    static boost::json::value combinations_map_to_json(const std::map<Utils::CoeffVarSet, double>& comb_to_double_map);
    static boost::json::value combinations_map_to_json(const std::map<Utils::CoeffVarPairSet, double>& comb_to_double_map);
    static boost::json::value combinations_to_json(const std::set<CoeffVarSet>& combinations);
    static boost::json::value combinations_to_json(const std::set<CoeffVarPairSet>& combinations);
    static boost::json::value lin_cons_problem_to_json(const LinConstraintsProblem& lin_cons_problem);
    static boost::json::value quad_cons_problem_to_json(const QuadConstraintsProblem& lin_cons_problem);
};

// Template function to extract keys from a map
template <typename K, typename V>
std::set<K> Utils::get_map_keys(const std::map<K, V>& input_map) {
    std::set<K> keys;
    for (const auto& pair : input_map) {
        keys.insert(pair.first);
    }
    return keys;
}

// Template function to extract values from a map
template <typename K, typename V>
std::set<V> Utils::get_map_values(const std::map<K, V>& input_map) {
    std::set<V> values;
    for (const auto& pair : input_map) {
        values.insert(pair.second);
    }
    return values;
}


#endif
