#ifndef GB_QUAD_CONSTRAINT_SOLVER_HPP
#define GB_QUAD_CONSTRAINT_SOLVER_HPP

#include "quad_constraints_solver.hpp"

// Derived class that uses Gurobi for constraint solving
class GbQuadConstraintsSolver : public QuadConstraintsSolver {
public:
    using QuadConstraintsSolver::QuadConstraintsSolver;

    QuadConstraintsSolution get_lower_bounds_for_combinations(const QuadConstraintsProblem& quad_constraints_map) override;

protected:
    void set_params(const std::unordered_map<std::string, std::string>& parameters) override;

private:
    bool use_parallel_mode = true;
    int num_threads = 8;
    double tl_per_comb = 1;
    int log_output_flag = 0;
};

extern "C" QuadConstraintsSolver* create_solver() {
    return new GbQuadConstraintsSolver();
}

#endif