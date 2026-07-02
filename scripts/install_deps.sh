#!/usr/bin/env bash
set -euo pipefail

# Resolve the directory where THIS script lives
SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Project root = parent of scripts/
PROJECT_ROOT="$( dirname "$SCRIPT_DIR" )"

##
# Global configs and paths
##
DEPS_DIR="$PROJECT_ROOT/deps"
DEPS_INSTALL_DIR="${DEPS_DIR}/install"

##
# Helper functions
##
log() { printf "\033[1;34m[build]\033[0m %s\n" "$*"; }
die() { printf "\033[1;31m[error]\033[0m %s\n" "$*" >&2; exit 1; }


get_boost() {
    ##
    # Install Boost 1.8
    ##
    BOOST_INSTALL_DIR=${DEPS_INSTALL_DIR}/boost

    # Download
    wget https://archives.boost.io/release/1.80.0/source/boost_1_80_0.tar.gz
    tar xf boost_1_80_0.tar.gz
    pushd boost_1_80_0 >/dev/null

    # Bootstrap build system
    ./bootstrap.sh --prefix=${BOOST_INSTALL_DIR}

    # Build & install
    ./b2 --without-python install -j4

    log "Installed Boost!"
    popd >/dev/null

    # Delete the tar file and unzipped folder after build and installation
    rm -rf boost_1_80_0
    rm -rf boost_1_80_0.tar.gz
}

get_gurobi() {
    ##
    # Install Gurobi 11.03
    ##
    GUROBI_INSTALL_DIR=${DEPS_INSTALL_DIR}/gurobi

    # Get the tar and unzip it
    wget https://packages.gurobi.com/11.0/gurobi11.0.3_linux64.tar.gz
    tar -xvzf gurobi11.0.3_linux64.tar.gz
    mv gurobi1103 ${GUROBI_INSTALL_DIR}
    log "Installed Gurobi!"

    # Remove the tar afer use
    rm -rf gurobi11.0.3_linux64.tar.gz
}

get_libtorch() {
    ##
    # Install libtorch
    ##
    LIBTORCH_INSTALL_DIR=${DEPS_INSTALL_DIR}/libtorch

    # Get and unzip
    wget https://download.pytorch.org/libtorch/cpu/libtorch-shared-with-deps-2.3.1%2Bcpu.zip
    unzip libtorch-shared-with-deps-2.3.1+cpu.zip
    mv libtorch ${LIBTORCH_INSTALL_DIR}
    log "Installed Libtorch!"

    # Delete the zip after use
    rm -rf libtorch-shared-with-deps-2.3.1+cpu.zip
}

get_z3() {
    ##
    # Install Z3
    ##
    Z3_SRC="z3-src"
    Z3_INSTALL_DIR=${DEPS_INSTALL_DIR}/z3

    # Clone the repo, checkout the desired version and build
    git clone https://github.com/Z3Prover/z3 ${Z3_SRC}
    pushd ${Z3_SRC} >/dev/null
    git checkout z3-4.15.3
    python3 scripts/mk_make.py --prefix=${Z3_INSTALL_DIR}
    cd build
    make -j4
    make install
    log "Installed Z3!"
    popd >/dev/null

    # Delete the src after build is complete
    rm -rf ${Z3_SRC}
}

main() {
    mkdir -p ${DEPS_DIR}
    mkdir -p ${DEPS_INSTALL_DIR}
    pushd ${DEPS_DIR} >/dev/null
    get_boost
    get_gurobi
    get_libtorch
    get_z3
    popd >/dev/null
}

main "$@"