#include "quad_affine_block.hpp"
#include "utils.hpp"
#include <iostream>

Utils::CoeffVarPairSet QuadAffineBlock::get_cvp_set_for_var(int v)
{
    if (assignments_map.find(v) != assignments_map.end()) {
        // Return the mapping for v which is stored in the assignments map
        return assignments_map[v];
    }
    else {
        // Return the identity set if there is no mapping from v
        return {{Utils::CoeffVar(1, v), Utils::CoeffVar(1, -1)}};
    }
}

void QuadAffineBlock::set_elina_linexpr(int assigned_var, elina_linexpr0_t* linexpr)
{
    // Parse the linexpr
    Utils::CoeffVarSet cv_set = Utils::parse_linexpr(linexpr);

    // Go over the linexpr and perform the corresponding operations and accumulate them
    Utils::CoeffVarPairSet accumulated_set, this_set;
    for (auto const&[coeff, var]: cv_set) {
        this_set = Utils::mult_cvp_set_k(get_cvp_set_for_var(var), coeff);
        accumulated_set = Utils::add_cvp_sets(accumulated_set, this_set);
    }

    // Set the accumulated cvpset to the assigned variable
    assignments_map[assigned_var] = accumulated_set;
}

void QuadAffineBlock::set_elina_linexprs_prod(int assigned_var, elina_linexpr0_t* linexpr1, elina_linexpr0_t* linexpr2)
{
    // Parse the linear expressions
    Utils::CoeffVarSet cv_set1 = Utils::parse_linexpr(linexpr1);
    Utils::CoeffVarSet cv_set2 = Utils::parse_linexpr(linexpr2);

    // Go over the linear expressions, perform the corresponding operations and accumulate them
    // to get the resultant expressions
    Utils::CoeffVarPairSet this_set, accumulated_set1, accumulated_set2;
    for (auto const&[coeff, var]: cv_set1) {
        this_set = Utils::mult_cvp_set_k(get_cvp_set_for_var(var), coeff);
        accumulated_set1 = Utils::add_cvp_sets(accumulated_set1, this_set);
    }

    for (auto const&[coeff, var]: cv_set2) {
        this_set = Utils::mult_cvp_set_k(get_cvp_set_for_var(var), coeff);
        accumulated_set2 = Utils::add_cvp_sets(accumulated_set2, this_set);
    }

    auto [final_cvp_set, is_result_quadratic] = Utils::multiply_cvp_sets(accumulated_set1, accumulated_set2);

    // Set the product cvp set to the assigned variable
    assignments_map[assigned_var] = final_cvp_set;
    this->is_linear = this->is_linear && !is_result_quadratic;
}

int QuadAffineBlock::size()
{
    return assignments_map.size();
}

std::vector<int> QuadAffineBlock::get_assigned_keys()
{
    std::vector<int> keys;
    for (const auto& pair : assignments_map) {
        keys.push_back(pair.first);
    }
    return keys;
}

std::map<int, Utils::CoeffVarPairSet> QuadAffineBlock::get_assignments_map()
{
    return assignments_map;
}

bool QuadAffineBlock::is_block_linear()
{
    return this->is_linear;
}