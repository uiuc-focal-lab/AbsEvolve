#ifndef CONSTRAINTS_TEMPLATE_HPP
#define CONSTRAINTS_TEMPLATE_HPP

#include <memory>
#include <set>
#include <string>

#include "utils.hpp"

/**
 * Class to describe the template for the Template Constraint Domain being used.
 */
class ConstraintsTemplate
{
public:
    // Virtual destructor for proper cleanup of derived classes
    virtual ~ConstraintsTemplate() = default;

    // Static factory method to create a template based on the name
    static std::unique_ptr<ConstraintsTemplate> create_template(const std::string& template_name);

    // Main method that returns the new combinations to be computed based on all the variables and modified variables.
    virtual std::set<Utils::CoeffVarSet> get_combinations_to_compute(
        const std::set<int>& all_variables,
        const std::set<int>& modified_variables
    ) = 0;
};

#endif