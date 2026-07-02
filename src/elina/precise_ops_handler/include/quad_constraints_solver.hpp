#ifndef QUAD_CONSTRAINTS_SOLVER_HPP
#define QUAD_CONSTRAINTS_SOLVER_HPP

#include <map>
#include <memory>
#include <unordered_map>

#include "utils.hpp"

class QuadConstraintsProblem
{
    public:
        std::set<int> variables;
        std::map<int, std::pair<double, double>> var_bounds_map;
        std::map<Utils::CoeffVarSet, double> constraints_map;
        std::set<Utils::CoeffVarPairSet> combinations_to_solve;  // expressions whose lower bounds are to be computed
};

class QuadConstraintsSolution
{
    public:
        std::map<Utils::CoeffVarPairSet, double> lower_bounds_map;
};

// Base class for constraint solvers with quadratic objectives.
class QuadConstraintsSolver {
public:
    // Virtual destructor for proper cleanup of derived classes
    virtual ~QuadConstraintsSolver() = default;

    // Static factory method to create a solver based on the name
    static std::unique_ptr<QuadConstraintsSolver> create_solver(const std::string& solver_name,
                                                                const std::unordered_map<std::string, std::string>& parameters = {});

    // Pure virtual method to get lower bounds for quadratic combinations
    virtual QuadConstraintsSolution get_lower_bounds_for_combinations(const QuadConstraintsProblem& quad_constraints_map) = 0;

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