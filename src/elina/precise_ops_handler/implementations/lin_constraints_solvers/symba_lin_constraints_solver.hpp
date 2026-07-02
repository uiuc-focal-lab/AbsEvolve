#ifndef SYMBA_LIN_CONSTRAINT_SOLVER_HPP
#define SYMBA_LIN_CONSTRAINT_SOLVER_HPP

#include "lin_constraints_solver.hpp"

// Derived class that uses the Symba solver to solve constraints.
class SymbaLinConstraintsSolver : public LinConstraintsSolver {
public:
    using LinConstraintsSolver::LinConstraintsSolver;

    LinConstraintsSolution get_lower_bounds_for_combinations(const LinConstraintsProblem& lin_constraints_problem) override;

protected:
    void set_params(const std::unordered_map<std::string, std::string>& parameters) override;

private:
    bool use_parallel_mode = false;
    int num_threads = 4;
};

extern "C" LinConstraintsSolver* create_solver() {
    return new SymbaLinConstraintsSolver();
}

#endif
