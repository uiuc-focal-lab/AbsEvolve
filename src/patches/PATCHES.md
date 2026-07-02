# Patch Overview

Use `generate_patches.sh` to regenerate the `.patch` files from the current subproject sources, and `apply_patches.sh` to apply them to a fresh checkout.

This document summarizes the changes introduced by each patch at a high level.
Each patch is applied to its respective subproject (`elina`, `crab`, `clam`) in that order.

---

## `elina.patch`

**Repository:** `src/elina` — https://github.com/eth-sri/ELINA @ `f524156d292ac3a6f3cd676e2d2e7db6629e2b6f`

> ELINA is a C library for efficient numerical abstract interpretation. It
provides optimised implementations of the Zones, Octagon, and Polyhedra abstract
domains (opt_zones, opt_oct, opt_pk) used by Crab as its underlying numerical
back-end when configured with `--enable-elina`.

This patch adds the `precise_ops_handler` sub-library to ELINA, which implements
precise quadratic-affine transformer support for all three numerical domains, and
fixes several pre-existing bugs in ELINA's Zones domain and partitions API.

### Changes

- **`precise_ops_handler/`** *(new sub-library, built as `libprecise_ops.so`)*
  Implements the full solving stack invoked by `opt_*_quad_affine()`:
  - **Core data structures** (`include/`): `QuadAffineBlock`, `QuadAffineProblem`,
    `QuadAffineSolution`, `QuadAffineSolver`, `LinConstraintsProblem`,
    `LinConstraintsSolution`, `LinConstraintsSolver`, `QuadConstraintsProblem`,
    `QuadConstraintsSolution`, `QuadConstraintsSolver`, `ConstraintsTemplate`, and
    `Utils` (arithmetic on `CoeffVarSet` / `CoeffVarPairSet` expressions).
  - **Linear solver implementations** (`implementations/lin_constraints_solvers/`):
    - `gb_lin_constraints_solver` — Gurobi LP, sequential and parallel modes.
    - `dual_lin_constraints_solver` — Lagrangian dual ascent via LibTorch (Adam).
    - `symba_lin_constraints_solver` — SMT-based via Z3 and the Symba solver;
      writes problems to a temp file and parses the Symba output.
  - **Quadratic solver implementations** (`implementations/quad_constraints_solvers/`):
    - `gb_quad_constraints_solver` — Gurobi non-convex QP via a linearising
      auxiliary variable trick, sequential and parallel modes.
    - `dual_quad_constraints_solver` — Lagrangian dual ascent with additional
      quadratic split parameters (S and D) optimised jointly via LibTorch.
  - **Constraint templates** (`implementations/constraints_templates/`):
    `OctagonTemplate` and `ZonesTemplate` define which linear combinations
    (±xᵢ, ±xᵢ ± xⱼ) to compute after each assignment block.
  - **CMake build system** (`CMakeLists.txt`, `cmake/FindGurobi.cmake`,
    `cmake/FindLibtorch.cmake`, `cmake/FindZ3.cmake`): discovers Gurobi,
    LibTorch, Z3, and Boost.JSON; builds each solver as a separate shared
    library loaded at runtime via `dlopen`.

- **`elina_oct/opt_oct_precise_ops.cpp` / `.hpp`** *(new)*
  Implements `opt_oct_quad_affine()`: constructs the `QuadAffineProblem` from
  the current octagon element, splits it into per-component sub-problems using
  the octagon's sparse component structure, forgets assigned dimensions, solves
  each sub-problem, and meets the results back into the abstract element.

- **`elina_poly/opt_pk_precise_ops.cpp` / `.hpp`** *(new)*
  Implements `opt_pk_quad_affine()`: same flow as the octagon version, but
  extracts variable bounds from the polyhedra box as well (polyhedra do not
  expose sparse components, so all variables are treated as one component).

- **`elina_zones/opt_zones_precise_ops.cpp` / `.hpp`** *(new)*
  Implements `opt_zones_quad_affine()`: same flow as the octagon version,
  using the Zones domain's sparse component structure.

- **`Makefile`** — adds `precise_ops_handler` build, install, and clean targets
  under the `IS_AFFINE=1` flag.

- **`configure`** — adds the `-use-affine` configuration flag;
  `GUROBI_PREFIX`, `LIBTORCH_PREFIX`, and `Z3_PREFIX` environment variables;
  and a macOS-specific compiler workaround (disables `-O3`, adds
  `-Wno-incompatible-pointer-types` for Clang compatibility).

- **`elina_oct/Makefile`**, **`elina_poly/Makefile`**, **`elina_zones/Makefile`**
  — link against `libprecise_ops` and `libboost_json` when `IS_AFFINE=1`.

---

## `crab.patch`

**Repository:** `src/crab` — https://github.com/seahorn/crab @ `146f5399c72ff508f176e6392e490647ac657ce7`

> Crab is a C++ library for building program static analyses based on Abstract
Interpretation. It provides abstract domains (Zones, Octagons, Polyhedra, etc.),
fixpoint solvers, and a CFG-based intermediate representation (CrabIR). Crab
integrates with external domain libraries such as Apron and Elina, and is used
as the analysis back-end for Clam.

This patch introduces the core `quad_affine_block` collation feature: the
ability to group sequences of related affine and quadratic arithmetic statements
into a single composite statement, then hand the entire block to a supporting
abstract domain solver for a more precise joint analysis.

### Changes

- **`include/crab/cfg/cfg.hpp`**
  Central change. Added:
  - `QUAD_AFFINE_BLOCK = 26` statement-code enum value.
  - `CollateStatus`, `AffinePrecisionLevel`, and `QuadPrecisionLevel` enums that
    control when collation is applied.
  - `quad_affine_block` statement class (template): stores a vector of binary
    operations together with their LHS variables and operand expressions; tracks
    def-use correctly so that variables defined earlier in the block are excluded
    from the use-set of later assignments.
  - Helper functions `checkAndAddInQuadSet()`, `shouldCollateStmt()`, and
    `collateStatementsIntoBlocks()` that implement the collation decision logic.
  - `basic_block::collateStatementsIntoQuadAffineBlocks()` and a CFG-level
    wrapper of the same name that apply collation to every basic block, driven by
    `AffinePrecisionLevel`, `QuadPrecisionLevel`, and a maximum block size.
  - `quad_affine_block_t` type alias and visitor support in `statement_visitor`.

- **`include/crab/domains/abstract_domain.hpp`**
  Added the `quad_affine_block()` virtual method to the abstract domain
  interface. The default implementation raises an error; only domains that
  explicitly support collated analysis override it.

- **`include/crab/domains/abstract_domain_macros.def`**
  Added an empty `quad_affine_block()` override for macro-generated domain
  wrappers (passthrough / unsupported).

- **`include/crab/domains/abstract_domain_params.hpp`**
  Extended `elina_domain_params` with `m_split_prob_in_comps`, `m_linear_solver_config`,
  and `m_quad_solver_config` fields that control the underlying solver behavior.

- **`include/crab/domains/elina_domains.hpp`**
  Implemented `quad_affine_block()` for Elina-backed domains (~154 lines):
  constructs a `QuadAffineBlock`, computes a baseline answer via standard Elina
  operations (required for soundness on polyhedra and non-linear blocks), then
  delegates to domain-specific handlers (`opt_zones_quad_affine`,
  `opt_oct_quad_affine`, `opt_pk_quad_affine`) and meets with the baseline.
  Also fixes `convert_elina_number()` for `z_number` to use `mpz_set_d()`
  instead of a direct cast.

- **`include/crab/domains/elina/elina.hpp`**
  Added includes for the three new precise-ops headers:
  `opt_zones_precise_ops.hpp`, `opt_oct_precise_ops.hpp`,
  `opt_pk_precise_ops.hpp`, and `quad_affine_block.hpp`.

- **`include/crab/domains/array_adaptive.hpp`**, **`array_smashing.hpp`**,
  **`combined_domains.hpp`**, **`flat_boolean_domain.hpp`**,
  **`generic_abstract_domain.hpp`**, **`region_domain.hpp`**
  Each wrapper domain adds `arith_operation_vector_t` / `lin_expr_vector_t`
  type aliases and a `quad_affine_block()` override that forwards the call
  to the wrapped base domain (with tag merging in `region_domain`,
  `reduce()` in product domains, etc.).

- **`include/crab/analysis/abs_transformer.hpp`**
  Added a `visit()` / `exec()` handler for `quad_affine_block_t` that
  converts the stored binary operations and calls `quad_affine_block()` on
  the current abstract state.

- **`include/crab/checkers/base_property.hpp`**
  Added `quad_affine_block_t` type and `check()` method so property checkers
  can traverse the new statement type.

- **`lib/abstract_domain_params.cpp`**
  Added `to_float()` and `to_unordered_map()` parsing utilities; wired the new
  `elina.split_prob_in_comps`, `elina.linear_solver_config`, and
  `elina.quad_solver_config` parameter keys into `update_params()` and
  `write()`.

- **`cmake/FindElina.cmake`**
  Extended to discover and link Elina's `precise_ops` library.

- **`.github/workflows/nightly-crab-docker.yaml`**,
  **`test-crab-dev-docker.yaml`**, **`test-crab-docker.yml`** *(deleted)*
  Removed CI/nightly Docker workflows that are not needed for the artifact.

---

## `clam.patch`

**Repository:** `src/clam` — https://github.com/seahorn/clam @ `9ce8172cb1658a62687d0420121e038f793ae4fc`

> Clam is an Abstract Interpretation-based static analyzer that computes
inductive invariants for LLVM bitcode. It acts as an LLVM frontend for the
Crab abstract interpretation library, translating LLVM IR into Crab's IR
(CrabIR) and invoking Crab's fixpoint engine over a chosen abstract domain.

This patch extends Clam's LLVM frontend to expose the new `quad_affine_block`
collation feature (introduced in `crab.patch`) as user-facing CLI options, and
adds Elina's zones domain as a first-class analysis choice.

### Changes

- **`include/clam/CfgBuilderParams.hh`**
  Added three new fields to `CrabBuilderParams`: `affine_prec_level`,
  `quad_prec_level`, and `max_collate_count`. Added helper methods
  `shouldCollateIntoQuadAffineBlocks()` and `maxCollateCount()`.

- **`include/clam/CrabDomain.hh`**
  Registered `ELINA_ZONES` as a new abstract domain (id 13), backed by
  Elina's opt_zones library. This is distinct from `ZONES_SPLIT_DBM`, which
  uses Crab's internal split-DBM representation.

- **`lib/Clam/CfgBuilder.cc`**
  After CFG simplification, conditionally calls
  `collateStatementsIntoQuadAffineBlocks()` on the built CFG when either
  precision level is non-default. Also extended the `write()` diagnostic
  output to print the new parameters.

- **`lib/Clam/Clam.cc`**
  Wires the three new CLI options into `CrabBuilderParams` when
  `ClamPass::runOnModule` is invoked.

- **`lib/Clam/ClamOptions.def`**
  Defines three new command-line options:
  - `--crab-affine-precision-level` (`default` | `affine-full`)
  - `--crab-quad-precision-level` (`default` | `quad-full`)
  - `--crab-max-collate-count` (integer; `-1` means maximal collation)
  Also adds `elina-zones` to the `--crab-dom` option.

- **`lib/Clam/RegisterAnalysis.cc`**
  Calls `register_elina_zones_domain()` during domain registration.

- **`lib/Clam/crab/domains/elina_zones.hh`** *(new file)*
  Defines `elina_zones_domain_t` as the full domain stack (region / array /
  bool+num wrappers) around Elina's zones base domain.

- **`lib/Clam/crab/domains/elina_zones.cc`** *(new file)*
  Registers `ELINA_ZONES` when `INCLUDE_ALL_DOMAINS` and `HAVE_ELINA` are
  both set; unregisters otherwise.

- **`lib/Clam/crab/domains/crab_domains.hh`** and **`register_domains.hh`**
  Include/declare the new `elina_zones` domain alongside the existing ones.

- **`py/clam.py`**
  Exposes `--crab-affine-precision-level`, `--crab-quad-precision-level`,
  and `--crab-max-collate-count` in the Python frontend, and adds
  `elina-zones` to the `--crab-dom` choices.
