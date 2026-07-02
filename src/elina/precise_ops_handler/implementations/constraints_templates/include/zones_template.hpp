#ifndef ZONES_TEMPLATE_HPP
#define ZONES_TEMPLATE_HPP

#include "constraints_template.hpp"

// Derived class that defines the zones domain template
class ZonesTemplate : public ConstraintsTemplate {
public:
    using ConstraintsTemplate::ConstraintsTemplate;

    std::set<Utils::CoeffVarSet> get_combinations_to_compute(
        const std::set<int>& all_variables,
        const std::set<int>& modified_variables
    ) override;
};

#endif