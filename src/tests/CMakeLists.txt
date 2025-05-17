set(TARGET_NAME unit_tests)

add_executable(${TARGET_NAME}
    ../bus_generator/goose_traffic_gen.hpp
    ../bus_generator/goose_traffic_gen.cpp

    ../bus_generator/sv_traffic_gen.hpp
    ../bus_generator/sv_traffic_gen.cpp

    ../bus_processor/process_bus_parser.hpp
    ../bus_processor/process_bus_parser.cpp

    goose_traffic_test.cpp
    sv_traffic_test.cpp

    main.cpp
)
target_link_libraries(${TARGET_NAME}
    PRIVATE
    gtest
    gmock
    gtest_main
)
setup_dpdk(${TARGET_NAME})
setup_libiec61850(${TARGET_NAME})

# CTest
add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
setup_libiec61850(${TARGET_NAME})
install(TARGETS ${TARGET_NAME} DESTINATION bin)

