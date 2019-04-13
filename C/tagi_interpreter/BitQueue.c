#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "BitQueue.h"

inline
uint8_t
SetBitsBelow(
    uint8_t Bit
    )
{
    //
    // We only care about uint8_t.
    //
    assert(Bit <= 8);

    uint8_t allBits = (uint8_t) ~0;
    uint8_t bitsBelow = (uint8_t) (~(allBits << Bit));
    return bitsBelow;
}

inline
uint8_t
MakeMask(
    uint8_t LowBit,
    uint8_t HighBit
    )
{
    assert(HighBit <= 8);
    assert(HighBit != 0);
    assert(LowBit < HighBit);

    uint8_t allBitsAboveLow = ~SetBitsBelow(LowBit);
    return (allBitsAboveLow & SetBitsBelow(HighBit));
}

inline
uint8_t
ExtractValue(
    uint8_t Raw,
    uint8_t LowBit,
    uint8_t HighBit
    )
{
    assert(HighBit <= 8);
    assert(HighBit != 0);
    assert(LowBit < HighBit);

    uint8_t mask = MakeMask(LowBit, HighBit);
    return ((Raw & mask) >> LowBit);
}

inline
int
AnyValidBits(
    Bits *Field
    )
{
    return (Field->Data.HighBit != 0);
}

/*
static
int
BitTest(
    void
    )
{
    Bits b;

    b.Data.Values = 0xff;
    b.Data.LowBit = 0;
    b.Data.HighBit = 8;

    int data = ExtractValue(b.Data.Values, b.Data.LowBit, b.Data.HighBit);
    printf("Data: %d, mask: %x\n", data, MakeMask(b.Data.LowBit, b.Data.HighBit));

    return b.Data.Values;
}
*/
