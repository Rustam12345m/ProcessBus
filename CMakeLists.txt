cmake_minimum_required(VERSION 3.10)
project(pbus LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

# Static DPDK via pkg-config
pkg_check_modules(DPDK_STATIC REQUIRED IMPORTED_TARGET libdpdk)

if (DPDK_STATIC_FOUND)
    # message(WARNING "DPDK libraries: ${DPDK_STATIC_LIBRARIES}")
    # message(WARNING "DPDK include dirs: ${DPDK_STATIC_INCLUDE_DIRS}")
    # message(WARNING "DPDK library dirs: ${DPDK_STATIC_LIBRARY_DIRS}")
    # message(WARNING "DPDK cflags: ${DPDK_STATIC_CFLAGS_OTHER}")
else()
    message(FATAL_ERROR "DPDK package not found!")
endif()
link_directories(${DPDK_STATIC_LIBRARY_DIRS})

# libiec61850: GOOSE/R-GOOSE/SV/R-SV
add_subdirectory(3rdparty/)

# Options: Intel Atom
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4 -Wno-unused-result -Wno-volatile")

# DPDK's related functions
function(setup_dpdk TARGET_NAME)
    target_include_directories(${TARGET_NAME}
        PRIVATE
        ${DPDK_STATIC_INCLUDE_DIRS}
	)

    target_compile_options(${TARGET_NAME}
        PRIVATE
        ${DPDK_STATIC_CFLAGS}
        -march=atom          # Target architecture
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
        -Wl,--whole-archive
        ${DPDK_STATIC_LIBRARIES}
        -lrte_mempool_ring   # 
        -lrte_bus_pci        # PCI bus driver
        -lrte_bus_vdev       # 
        -lrte_net_virtio     # NIC's PMD
        -lrte_net_e1000      #
        -lrte_net_igc        #
        -lrte_net_ixgbe      # 
        -lrte_net_i40e       #
        -lrte_net_af_xdp     #
        -lrte_net_af_packet  #
        -lrte_net_ring       #
        -lrte_net_tap        #
        -lrte_net_vhost
        -Wl,--no-whole-archive
        pcap
        elf
        bpf
        xdp
        numa
    )
endfunction()

# Unit tests with Googletest
option(BUILD_TESTS "Build unit tests" ON)
if (BUILD_TESTS)
    add_subdirectory(3rdparty/googletest EXCLUDE_FROM_ALL)

    include_directories(
        3rdparty/googletest/googletest/include
        3rdparty/googletest/googlemock/include
        3rdparty/cxxopts/include
    )

    include(CTest)
    enable_testing()
endif()

# Apps
add_subdirectory(src/)

