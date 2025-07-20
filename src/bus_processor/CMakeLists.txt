set(TARGET_NAME bus_processor)

add_executable(${TARGET_NAME}
    ../common/utils.hpp
    ../common/utils.cpp

    process_bus_parser.hpp
    process_bus_parser.cpp

    rx_application.hpp
    rx_application.cpp

    main.cpp
)

setup_dpdk(${TARGET_NAME})
install(TARGETS ${TARGET_NAME} DESTINATION bin)

