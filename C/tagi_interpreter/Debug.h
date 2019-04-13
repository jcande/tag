#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "Util.h"
#include "Blob.h"
#include "Tag.h"

typedef struct _SymbolEntry
{
    uint64_t RawSymbol;

    char *Name;

    tommy_node Node;
} SymbolEntry;

struct _TagSystem;

typedef struct _Debugger
{
    //
    // This is a map from RawSymbol -> Name
    //
    tommy_hashlin SymbolMap;

    //
    // This is the "CPU" that we are to debug. Code is located in TagSystem::
    //
    struct _TagSystem *System;
} Debugger;

//
// This is meant to be stored in each instruction. None of the data in this is
// owned by this structure.
// XXX we should try to implement a gdb-stub and we also need to manage
// stdin/stdout for the tag program (so distinct from the stdin/stdout the
// debugger uses)
//
typedef struct _DebugData
{
    SymbolEntry *Info;

    //
    // If true, a breakpoint is set.
    //
    bool Break;
} DebugData;


STATUS
ReadSymbolFile(
    FILE *fp,
    SymbolEntry **Entries,
    uint64_t *Count
    );

STATUS
DebuggerInitialize(
    Debugger *Dbg,
    SymbolEntry *Entries,
    uint64_t Count
    );

void
SymbolEntryTeardown(
    void *Entry
    );

void
SymbolEntryTeardownComplete(
    void *Entry
    );

void
DebuggerTeardown(
    Debugger *Dbg
    );
