#pragma once

#include <stdint.h>
#include <vector>

template< typename T >
struct TxBlock
{
    std::vector< T > packets;
};

template< typename T >
struct TxUnit
{
    typedef TxBlock< T > BlockType;

    uint64_t offsetUS = 0; // Timestamp

    std::vector< BlockType >  blocks;
};

