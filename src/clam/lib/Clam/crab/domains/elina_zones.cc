#include <clam/config.h>
#include <clam/CrabDomain.hh>
#include <clam/RegisterAnalysis.hh>
#include <crab/config.h>
#include "elina_zones.hh"

namespace clam {
// Only register when both all-domains mode is on AND Elina is available.
// In all other cases, unregister so queries for ELINA_ZONES fall back gracefully.
#ifdef INCLUDE_ALL_DOMAINS
#if defined(HAVE_ELINA)
REGISTER_DOMAIN(clam::CrabDomain::ELINA_ZONES, elina_zones_domain)
#else
UNREGISTER_DOMAIN(elina_zones_domain)
#endif
#else
UNREGISTER_DOMAIN(elina_zones_domain)
#endif
} // end namespace clam

