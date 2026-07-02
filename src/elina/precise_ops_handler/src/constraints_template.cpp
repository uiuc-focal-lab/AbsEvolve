#include <stdexcept>  

#include "constraints_template.hpp"
#include "octagon_template.hpp"
#include "zones_template.hpp"

std::unique_ptr<ConstraintsTemplate> ConstraintsTemplate::create_template(const std::string& template_name)
{
    if (template_name == "oct") {
        return std::make_unique<OctagonTemplate>();
    } else if (template_name == "zones") {
        return std::make_unique<ZonesTemplate>();
    } else {
        throw std::invalid_argument("Unknown template type: " + template_name);
    }    
}