#ifndef QUAD_AFFINE_SOLVER_HPP
#define QUAD_AFFINE_SOLVER_HPP

#include <map>
#include <string>

#include "lin_constraints_solver.hpp"
#include "quad_constraints_solver.hpp"
#include "utils.hpp"

class QuadAffineProblem
{
    public:
        std::set<int> variables;
        std::map<int, std::pair<double, double>> var_bounds_map;
        std::map<Utils::CoeffVarSet, double> constraints_map;
        std::map<int, Utils::CoeffVarPairSet> assignments_map;  // var -> expression being assigned to it
        std::string constraints_template_name;  // selects which ConstraintsTemplate subclass to use (e.g. "oct", "zones")

        std::vector<QuadAffineProblem> split_problem_in_components(std::vector<std::vector<int>> var_components);
};

class QuadAffineSolution
{
    public:
        std::map<Utils::CoeffVarSet, double> new_constraints;
};

class QuadAffineSolver {
    public:
        QuadAffineSolver(const std::unordered_map<std::string, std::string>& lin_solver_config, const std::unordered_map<std::string, std::string>& quad_solver_config);

        QuadAffineSolution solve_quad_affine(const QuadAffineProblem& quad_affine_problem);
    private:
        bool m_log_linear_problems_as_json = false;    // set via "log" key in lin_solver_config
        bool m_log_quadratic_problems_as_json = false;  // set via "log" key in quad_solver_config
        std::unique_ptr<LinConstraintsSolver> m_lin_constraint_solver;
        std::unique_ptr<QuadConstraintsSolver> m_quad_constraint_solver;
        std::unordered_map<std::string, std::string> m_lin_solver_config;
        std::unordered_map<std::string, std::string> m_quad_solver_config;

        std::map<Utils::CoeffVarSet, Utils::CoeffVarPairSet>
        get_updated_combinations_after_affine(
            const std::set<Utils::CoeffVarSet>& combinations_to_compute,
            const std::map<int, Utils::CoeffVarPairSet>& assignment_map
        );

        bool is_prob_linear(std::map<Utils::CoeffVarSet, Utils::CoeffVarPairSet> updated_combinations_map);
        std::map<Utils::CoeffVarSet, Utils::CoeffVarSet> linearize(std::map<Utils::CoeffVarSet, Utils::CoeffVarPairSet> updated_combinations_map);

};

#endif