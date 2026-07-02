#ifndef OCTAGON_TEMPLATE_HPP
#define OCTAGON_TEMPLATE_HPP

#include "constraints_template.hpp"

// Derived class that defines the octagon domain template
class OctagonTemplate : public ConstraintsTemplate {
public:
    using ConstraintsTemplate::ConstraintsTemplate;

    std::set<Utils::CoeffVarSet> get_combinations_to_compute(
        const std::set<int>& all_variables,
        const std::set<int>& modified_variables
    ) override;
};

#endif