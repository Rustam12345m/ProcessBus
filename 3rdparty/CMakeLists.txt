# Configure libiec61850 submodule
set(WITH_MBEDTLS3                   ON CACHE BOOL "on")
set(CONFIG_INCLUDE_GOOSE_SUPPORT    ON CACHE BOOL "on")
set(CONFIG_IEC61850_R_SMV           ON CACHE BOOL "on")
set(CONFIG_IEC61850_L2_SMV          ON CACHE BOOL "on")
set(CONFIG_IEC61850_R_GOOSE         ON CACHE BOOL "on")
set(CONFIG_IEC61850_L2_GOOSE        ON CACHE BOOL "on")
set(BUILD_EXAMPLES                  OFF CACHE BOOL "off")
set(CONFIG_IEC61850_SNTP_CLIENT     OFF CACHE BOOL "off")
# set(DEBUG_GOOSE_SUBSCRIBER TRUE)
# set(DEBUG_GOOSE_PUBLISHER TRUE)
add_subdirectory(libiec61850 EXCLUDE_FROM_ALL)

function(setup_libiec61850 TARGET_NAME)
    target_include_directories(${TARGET_NAME}
        PRIVATE
        ${CMAKE_SOURCE_DIR}/3rdparty/libiec61850/src/common/inc/
        ${CMAKE_SOURCE_DIR}/3rdparty/libiec61850/src/iec61850/inc/
        ${CMAKE_SOURCE_DIR}/3rdparty/libiec61850/src/mms/inc/
        ${CMAKE_SOURCE_DIR}/3rdparty/libiec61850/src/logging/
        ${CMAKE_SOURCE_DIR}/3rdparty/libiec61850/src/goose/
        ${CMAKE_SOURCE_DIR}/3rdparty/libiec61850/src/r_session/
        ${CMAKE_SOURCE_DIR}/3rdparty/libiec61850/src/sampled_values/
        ${CMAKE_SOURCE_DIR}/3rdparty/libiec61850/hal/inc/
    )
    # message(WARNING "setup_libiec61850 include: ${CMAKE_SOURCE_DIR}")

    target_link_libraries(${TARGET_NAME}
        PUBLIC
        iec61850
    )
endfunction()

