#pragma once

#include <stdint.h>

uint8_t
SetBitsBelow(
    uint8_t Bit
    );

uint8_t
MakeMask(
    uint8_t LowBit,
    uint8_t HighBit
    );

uint8_t
ExtractValue(
    uint8_t Raw,
    uint8_t LowBit,
    uint8_t HighBit
    );

typedef union _Bits
{
    struct {
        uint8_t Values;

        //
        // This is the offset (inclusive).
        //
        uint8_t LowBit      : 4;
        //
        // This is the max (exclusive).
        //
        uint8_t HighBit     : 4;
    } Data;

    uint16_t AsUint16;
} Bits;


int
AnyValidBits(
    Bits *Field
    );
