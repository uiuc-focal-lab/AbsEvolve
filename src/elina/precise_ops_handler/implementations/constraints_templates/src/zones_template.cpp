#include "zones_template.hpp"

std::set<Utils::CoeffVarSet> ZonesTemplate::get_combinations_to_compute(
        const std::set<int>& all_variables,
        const std::set<int>& modified_variables
    )
{
    std::set<Utils::CoeffVarSet> combinations_to_compute;
    Utils::CoeffVarSet inner_set;

    // Individual bounds of the modified variables.
    for (int modified_var: modified_variables) {
        inner_set = {Utils::CoeffVar(1, modified_var)};
        combinations_to_compute.insert(inner_set);

        inner_set = {Utils::CoeffVar(-1, modified_var)};
        combinations_to_compute.insert(inner_set);
    }

    // Linear relation bounds (only of the form x - y and -x + y) of modified variables with all other variables.
    for (auto it1 = all_variables.begin(); it1 != all_variables.end(); ++it1) {
        for (auto it2 = std::next(it1); it2 != all_variables.end(); ++it2) {
            int vari = *it1;
            int varj = *it2;

            // Check if both vari and varj are not in updated_vars
            if (modified_variables.find(vari) == modified_variables.end() && 
                modified_variables.find(varj) == modified_variables.end()) {
                continue;  // Skip this combination
            }

            // Add the combinations to the vector
            inner_set = {Utils::CoeffVar(1, vari), Utils::CoeffVar(-1, varj)};
            combinations_to_compute.insert(inner_set);

            inner_set = {Utils::CoeffVar(-1, vari), Utils::CoeffVar(1, varj)};
            combinations_to_compute.insert(inner_set);
        }
    }

    return combinations_to_compute;
}