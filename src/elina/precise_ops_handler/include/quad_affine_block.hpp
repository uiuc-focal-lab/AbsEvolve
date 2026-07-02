#ifndef QUAD_AFFINE_BLOCK_HPP
#define QUAD_AFFINE_BLOCK_HPP

#include "utils.hpp"
#include <map>
#include <vector>

class QuadAffineBlock
{
    public:
        void set_elina_linexpr(int assigned_var, elina_linexpr0_t* linexpr);
        void set_elina_linexprs_prod(int assigned_var, elina_linexpr0_t* linexpr1, elina_linexpr0_t* linexpr2);
        int size();
        std::vector<int> get_assigned_keys();
        std::map<int, Utils::CoeffVarPairSet> get_assignments_map();
        bool is_block_linear();  // returns false if any assignment introduced a quadratic term

    private:
        bool is_linear = true;  // tracks whether all assignments so far are linear
        std::map<int, Utils::CoeffVarPairSet> assignments_map;
        Utils::CoeffVarPairSet get_cvp_set_for_var(int v);  // returns assignment for v, or identity {1*v} if unassigned
};


#endif