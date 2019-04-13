#pragma once

#include "tommyhashlin.h"
#include "TagQueue.h"
#include "IoBuffer.h"
#include "TagRule.h"

#define TAGSS                   2
#define TAGSS_STATUS(Code)      (MAKE_STATUS(Code, TAGSS))

#define TAGSS_OK                0
#define TAGSS_BADRULECOUNT      (TAGSS_STATUS(1))
#define TAGSS_BADDELETIONNUMBER (TAGSS_STATUS(2))
#define TAGSS_OUTOFTAPE         (TAGSS_STATUS(3))
#define TAGSS_BADRULE           (TAGSS_STATUS(4))
#define TAGSS_HALT              (TAGSS_STATUS(5))
#define TAGSS_DUPERULE          (TAGSS_STATUS(6))

/*
 * We want to be able to run programs with a TON of productions. This means
 * that using a char to represent the symbols will not suffice. We will instead
 * use Blobs. The problem with Blobs is that it is unstructured. To deal with
 * that we will institute a "symbol size" variable. For programs with minimal
 * products, symbol size can be 1. This is equivalent to using char. Larger
 * programs will have 2-, 3-, or more byte symbol sizes. The implication is
 * that we need to verify appendants are multiples of the symbol size.
 */

typedef struct _TagSystem
{
    //
    // This can be thought of as code.
    //
    tommy_hashlin Productions;
    //
    // This can be thought of as data.
    //
    TagQueue Tape;

    Blob InitialQueue;

    uint32_t AbstractDeletionNumber;    // The deletion number without taking into account the byte-count of the symbols involved.
    uint32_t SymbolSize;    // The byte-count of the symbols involved.

    IoBuffer Io;

    //
    // TODO add some state here such as RUNNING, DEBUGGING, etc
    //
} TagSystem;

STATUS
TagInitialize(
    TagSystem *System,
    TagRule *Rules,
    uint64_t RuleSize,
    Blob *DefaultQueue,
    uint32_t AbstractDeletionNumber,
    IoBufferConfig *Io
    );

void
TagTeardown(
    TagSystem *System
    );

/*

STATUS
TagRun(
    TagSystem *System
    );

// states include: PAUSE, STOP, RUN
void
TagStateChange(
    STATE NewState
    );

*/

STATUS
TagStep(
    TagSystem *System
    );

void
TagDump(
    TagSystem *System
    );
