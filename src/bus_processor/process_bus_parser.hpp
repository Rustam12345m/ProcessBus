#pragma once

#include "common/goose_container.hpp"
#include "common/sv_container.hpp"

/**
 * @function parse_goose_packet
 */
int parse_goose_packet(const uint8_t *buffer, int size,
                       GoosePassport &passport, GooseState &state);

/**
 * @function parse_sv_packet
 */
int parse_sv_packet(const uint8_t *buffer, int size,
                    SVStreamPassport &passport, SVStreamState &state);

