#pragma once

#include "RingBuffer.h"
#include "Blob.h"

typedef struct _CyclicTag
{
    RingBuffer Tape;

    Blob **Appendants;
    uint64_t AppendantsSize;

    uint64_t Marker;
} CyclicTag;

int
CyclicTagInitialize(
    CyclicTag *Config,
    Blob **Appendants,
    uint64_t AppendantsSize,
    uint64_t MaxTape
    );

int
CyclicTagWriteTape(
    CyclicTag *Config,
    Blob *Contents
    );

int
CyclicTagStep(
    CyclicTag *Config
    );

void
CyclicTagDump(
    CyclicTag *Config
    );
