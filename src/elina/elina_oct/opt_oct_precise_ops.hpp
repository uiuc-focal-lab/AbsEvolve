#ifndef __OPT_OCT_PRECISE_OPS_H
#define __OPT_OCT_PRECISE_OPS_H

#include "elina_abstract0.h"
#include "quad_affine_block.hpp"

#include <unordered_map>

#ifdef __cplusplus
extern "C" {
#endif

elina_abstract0_t* opt_oct_quad_affine(elina_manager_t* man,
                                       elina_abstract0_t* a,
                                       QuadAffineBlock& quad_affine_block,
                                       bool split_problem_in_comps,
                                       std::unordered_map<std::string, std::string> linear_solver_config,
                                       std::unordered_map<std::string, std::string> quad_solver_config);
#ifdef __cplusplus
}
#endif

#endif