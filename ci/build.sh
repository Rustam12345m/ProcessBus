#!/bin/bash
# CI building application

set -e # Exit on error

SCRIPT_PATH="$(dirname "$(realpath "$0")")"
REPO_DIR="$SCRIPT_PATH/../"
BUILD_DIR="$REPO_DIR/build/"
INSTALL_DIR="$REPO_DIR/install/"
DPDK_DIR="$REPO_DIR/3rdparty/dpdk/"
DPDK_INSTALL="$DPDK_DIR/build/install/"
DPDK_PKGCONFIG="$DPDK_INSTALL/lib/x86_64-linux-gnu/pkgconfig/"

function build_dpdk()
{
    cd $DPDK_DIR
    rm -rf build/

    # Building: x86 Intel Atom + Intel NICs
    meson setup build \
        --prefix="$DPDK_DIR/build/install/" \
        -Dmachine=atom \
        -Dbuildtype=release \
        -Dmax_numa_nodes=1 \
        -Ddisable_drivers=all \
        -Denable_drivers=net_e1000,net_igc,net_ixgbe,net_ice,net_af_xdp,net_tap,net_virtio,net_ring,net_bpf,net_vhost

    # --cross-file $REPO_DIR/devices/orangepi3b/meson-rk3566-toolchain.cross

    # Install
    ninja -C build
    ninja -C build install

    # Hack to force static linking
    rm -rf "$DPDK_DIR/build/install/lib"/x86_64-linux-gnu/*.so
    rm -rf "$DPDK_DIR3rdparty/dpdk/build/install/lib"/x86_64-linux-gnu/*.so.*

    # Check it
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$DPDK_PKGCONFIG"
    local FOUND=$(pkg-config --list-all | grep dpdk)
    [ -z "$FOUND" ] && { echo "Can't find DPDK!" ; exit 1 ; }
}

function build_apps()
{
    cd "$REPO_DIR"
    # 	rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"

    export PKG_CONFIG_PATH="$DPDK_PKGCONFIG:$PKG_CONFIG_PATH"

    cmake -S ./ -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=ON \
        -DBUILD_SAMPLES=OFF \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DCMAKE_CXX_FLAGS_RELEASE="-O3 -msse3" \
        -DCMAKE_C_FLAGS_RELEASE="-O3 -msse3"
    
    #-DCMAKE_TOOLCHAIN_FILE=$REPO_DIR/devices/orangepi3b/rk3566-toolchain.cmake

    cmake --build "$BUILD_DIR"
    cmake --install "$BUILD_DIR"

    # DPDP's stuff
    cp "$REPO_DIR"/3rdparty/dpdk/usertools/dpdk-devbind.py "$INSTALL_DIR"
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

prepare_sources
build_dpdk
build_apps

