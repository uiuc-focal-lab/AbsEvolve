#pragma once

// Zones domain backed by Elina's optimized zone library (opt_zones).
// Unlike ZONES_SPLIT_DBM (which uses a split-DBM representation internal to
// Crab), this domain delegates all operations to Elina and thus supports
// the quad_affine_block precise-ops interface.
// Requires HAVE_ELINA to be set at build time.

#include <crab/domains/elina_domains.hpp>
#include "crab_defs.hh"

namespace clam {
#ifdef HAVE_ELINA
using BASE(elina_zones_domain_t) =
  crab::domains::elina_domain<number_t, region_subdom_varname_t,
			      crab::domains::elina_domain_id_t::ELINA_ZONES>;
#endif
// Full domain stack: region wrapper around array wrapper around bool+num wrapper
// around the base Elina zones domain.
using elina_zones_domain_t = RGN_FUN(ARRAY_FUN(BOOL_NUM(BASE(elina_zones_domain_t))));
} // end namespace clam
