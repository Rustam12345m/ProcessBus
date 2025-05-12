#pragma once

#include "common/goose_container.hpp"

/**
 * @function parse_goose_packet
 */
int parse_goose_packet(const uint8_t *buffer, int size,
                       GoosePassport &passport, GooseState &state);

/**
 * @function is_goose
 * @brief Is used to get APPID and dispatch GOOSE mbuf to a particular CPU
 */
bool is_goose(const uint8_t* buffer, int size, uint16_t *appid);

