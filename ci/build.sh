#!/bin/bash
# Building generator and processor for ProcessBus

set -e # Exit on error

SCRIPT_PATH="$(dirname "$(realpath "$0")")"
REPO_DIR="$SCRIPT_PATH/../"
BUILD_DIR="$REPO_DIR/build/"
INSTALL_DIR="$REPO_DIR/install/"
DPDK_DIR="$REPO_DIR/3rdparty/dpdk/"
DPDK_INSTALL="$DPDK_DIR/build/install/"
DPDK_PKGCONFIG="$DPDK_INSTALL/lib/pkgconfig/"
TARGET_PROCESSOR=atom

OPT_UPDATE_SRC=1
OPT_BUILD_DPDK=1
OPT_BUILD_PBUS=1
OPT_REBUILD=0

function usage()
{
    echo "Usage: $0 [--update=0/1] [--dpdk=0/1] [--pbus=0/1] [--rebuild]"
    exit 1
}

function prepare_sources()
{
    # Ensure submodules are updated
    git submodule update --init --recursive

    # Download mbedtls for libiec61850
    MBEDTLS_DIR="$REPO_DIR/3rdparty/libiec61850/third_party/mbedtls/"
    wget -q -O "$MBEDTLS_DIR/mbedtls.tar.gz" https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v3.6.0.tar.gz
    tar xzf "$MBEDTLS_DIR/mbedtls.tar.gz" -C "$MBEDTLS_DIR"
    rm "$MBEDTLS_DIR/mbedtls.tar.gz"
}

function build_dpdk()
{
    cd $DPDK_DIR
    rm -rf build/

    # Building: x86 Intel Atom + Intel NICs
    meson setup build \
        --prefix="$DPDK_DIR/build/install/" \
        -Dlibdir=lib \
        -Dmachine=$TARGET_PROCESSOR \
        -Ddefault_library=static \
        -Dbuildtype=release \
        -Dmax_numa_nodes=1 \
        -Ddisable_drivers=all \
        -Denable_drivers=net_e1000,net_igc,net_ixgbe,net_ice,net_af_xdp,net_tap,net_virtio,net_ring,net_bpf,net_vhost

    # Install
    ninja -C build
    ninja -C build install

    # Hack to force static linking
    rm -rf "$DPDK_INSTALL"/lib/*.so
    rm -rf "$DPDK_INSTALL"/lib/*.so.*

    # Check it
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$DPDK_PKGCONFIG"
    if ! pkg-config --list-all | grep dpdk > /dev/null ; then
        echo "Can't find static libs for DPDK!"
        exit 1
    fi
}

function rebuild_and_install()
{
    cmake --build "$BUILD_DIR"
    cmake --install "$BUILD_DIR"

    # DPDP's stuff
    cp "$REPO_DIR"/3rdparty/dpdk/usertools/dpdk-devbind.py "$INSTALL_DIR"
}

function build_apps()
{
    cd "$REPO_DIR"
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"

    export PKG_CONFIG_PATH="$DPDK_PKGCONFIG:$PKG_CONFIG_PATH"

    cmake -S ./ -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=ON \
        -DBUILD_SAMPLES=OFF \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DCMAKE_CXX_FLAGS_RELEASE="-O3 -msse3" \
        -DCMAKE_C_FLAGS_RELEASE="-O3 -msse3"

    rebuild_and_install
}

# Options
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --update=*)
            OPT_UPDATE_SRC="${1#*=}"
            ;;
        --dpdk=*)
            OPT_BUILD_DPDK="${1#*=}"
            ;;
        --pbus=*)
            OPT_BUILD_PBUS="${1#*=}"
            ;;
        --rebuild)
            OPT_UPDATE_SRC=0
            OPT_BUILD_DPDK=0
            OPT_BUILD_PBUS=0
            OPT_REBUILD=1
            ;;
        *) usage;;
    esac
    shift
done

if [[ "$OPT_UPDATE_SRC" -eq 1 ]]; then
    echo "Performing source update..."
    prepare_sources
fi

if [[ "$OPT_BUILD_DPDK" -eq 1 ]]; then
    echo "Building DPDK..."
    build_dpdk
fi

if [[ "$OPT_BUILD_PBUS" -eq 1 ]]; then
    echo "Building PBUS..."
    build_apps
fi

if [[ "$OPT_REBUILD" -eq 1 ]]; then
    echo "Rebuild apps without cleaning..."
    rebuild_and_install
fi

