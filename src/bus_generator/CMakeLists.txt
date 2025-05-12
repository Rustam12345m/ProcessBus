set(TARGET_NAME bus_generator)

add_executable(${TARGET_NAME}
    goose_traffic_gen.hpp
    goose_traffic_gen.cpp

    sv_traffic_gen.hpp
    sv_traffic_gen.cpp

    ../common/utils.hpp
    ../common/utils.cpp

    main.cpp
)

target_include_directories(${TARGET_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)

setup_dpdk(${TARGET_NAME})
setup_libiec61850(${TARGET_NAME})

install(TARGETS ${TARGET_NAME} DESTINATION bin)

