#ifndef LIN_CONSTRAINTS_SOLVER_HPP
#define LIN_CONSTRAINTS_SOLVER_HPP

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include "utils.hpp"

class LinConstraintsProblem
{
    public:
        std::set<int> variables;
        std::map<int, std::pair<double, double>> var_bounds_map;
        std::map<Utils::CoeffVarSet, double> constraints_map;
        std::set<Utils::CoeffVarSet> combinations_to_solve;  // expressions whose lower bounds are to be computed
};

class LinConstraintsSolution
{
    public:
        std::map<Utils::CoeffVarSet, double> lower_bounds_map;
};

// Base class for constraint solvers with linear objectives.
class LinConstraintsSolver {
public:
    // Virtual destructor for proper cleanup of derived classes
    virtual ~LinConstraintsSolver() = default;

    // Static factory method to create a solver based on the name
    static std::unique_ptr<LinConstraintsSolver> create_solver(const std::string& solver_name,
                                                               const std::unordered_map<std::string, std::string>& parameters = {});

    // Pure virtual method to get lower bounds for linear combinations
    virtual LinConstraintsSolution get_lower_bounds_for_combinations(const LinConstraintsProblem& lin_constraints_problem) = 0;

protected:
    // Virtual method to parse and set parameters
    virtual void set_params(const std::unordered_map<std::string, std::string>& parameters)
    {
        // std::cout << "Base class parse_options called" << std::endl;
    }

private:
    static std::unordered_map<std::string, void*> loaded_libraries;  // Cache for loaded libraries
};

#endif
