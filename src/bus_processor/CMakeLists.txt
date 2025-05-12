set(TARGET_NAME bus_processor)
add_executable(${TARGET_NAME}
    goose_parser.hpp
    goose_parser.cpp

    ../common/utils.hpp
    ../common/utils.cpp

    main.cpp
)
setup_dpdk(${TARGET_NAME})
install(TARGETS ${TARGET_NAME} DESTINATION bin)

