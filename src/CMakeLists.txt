include_directories(
    ${CMAKE_SOURCE_DIR}/3rdparty/cxxopts/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/common/
    ${CMAKE_CURRENT_SOURCE_DIR}/dpdk_cpp/
    ${CMAKE_CURRENT_SOURCE_DIR}/bus_generator/
    ${CMAKE_CURRENT_SOURCE_DIR}/bus_processor/
)

add_subdirectory(bus_generator/)
add_subdirectory(bus_processor/)

if (BUILD_TESTS)
	add_subdirectory(tests/)
endif()

if (BUILD_SAMPLES)
    add_subdirectory(playground/)
endif()
