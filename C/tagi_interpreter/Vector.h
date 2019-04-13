#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "Util.h"

typedef void(*TeardownFn)(void*);

typedef struct _Vector
{
    uint8_t *Buffer;
    size_t BufferSize;
    size_t ActiveSize;
    uint16_t ElementSize; 
    TeardownFn Teardown;
} Vector;

STATUS
VectorInitialize(
    Vector *Vec,
    void *Buffer,
    size_t Count,
    uint16_t ElementSize,
    TeardownFn Teardown
    );

bool
VectorHasRoom(
    Vector *Vec
    );

void *
VectorBuffer(
    Vector *Vec
    );

size_t
VectorSize(
    Vector *Vec
    );

STATUS
VectorExpand(
    Vector *Vec,
    size_t MinimumSize
    );

STATUS
VectorAppend(
    Vector *Vec,
    void *Element
    );

// XXX not a real vector implementation but at least now we have an object to
// work on should we require more functionality

void
VectorTeardown(
    Vector *Vec
    );
