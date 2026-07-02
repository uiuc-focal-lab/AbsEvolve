#ifndef DUAL_QUAD_CONSTRAINT_SOLVER_HPP
#define DUAL_QUAD_CONSTRAINT_SOLVER_HPP

#include "quad_constraints_solver.hpp"
#include <torch/torch.h>

class DualQuadConstraintsSolver : public QuadConstraintsSolver {
public:
    using QuadConstraintsSolver::QuadConstraintsSolver;
    
    QuadConstraintsSolution get_lower_bounds_for_combinations(const QuadConstraintsProblem& quad_constraints_problem) override;

protected:
    void set_params(const std::unordered_map<std::string, std::string>& parameters) override;

private:
    double dual_lams_lr = 0.3;   // learning rate for dual variables (λ)
    double split_lams_lr = 0.3;  // learning rate for quadratic split parameters (S and D)
    int num_epochs = 5;
    int num_threads = 4;
    bool quad_initialization = true;
    torch::Device device = torch::kCPU;
};

extern "C" QuadConstraintsSolver* create_solver() {
    return new DualQuadConstraintsSolver();
}

#endif