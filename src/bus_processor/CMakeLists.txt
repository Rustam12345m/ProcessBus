set(TARGET_NAME bus_processor)

add_executable(${TARGET_NAME}
    process_bus_parser.hpp
    process_bus_parser.cpp

    ../common/utils.hpp
    ../common/utils.cpp

    main.cpp
)

setup_dpdk(${TARGET_NAME})
install(TARGETS ${TARGET_NAME} DESTINATION bin)

