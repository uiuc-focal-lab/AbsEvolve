#ifndef DUAL_LIN_CONSTRAINT_SOLVER_HPP
#define DUAL_LIN_CONSTRAINT_SOLVER_HPP

#include "lin_constraints_solver.hpp"
#include <torch/torch.h>

class DualLinConstraintsSolver : public LinConstraintsSolver {
public:
    using LinConstraintsSolver::LinConstraintsSolver;
    
    LinConstraintsSolution get_lower_bounds_for_combinations(const LinConstraintsProblem& lin_constraints_problem) override;

protected:
    void set_params(const std::unordered_map<std::string, std::string>& parameters) override;

private:
    double learning_rate = 0.5;
    int num_epochs = 5;
    int num_adaptive_learning_epochs = 5;
    int num_threads = 4;
    torch::Device device = torch::kCPU;
};

extern "C" LinConstraintsSolver* create_solver() {
    return new DualLinConstraintsSolver();
}

#endif