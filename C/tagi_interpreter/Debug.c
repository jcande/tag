#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <err.h>
#include <errno.h>
#include <unistd.h>

#include "tommyhashlin.h"

#include "Debug.h"
#include "Util.h"
#include "Blob.h"
#include "Vector.h"

// XXX This should be part of stdio.h but apparently not for clang.
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
// XXX This should be in string.h but apparently not for clang.
char *strdup(const char *str);

// XXX should this just take a vector?
STATUS
ReadSymbolFile(
    FILE *fp,
    SymbolEntry **Entries,
    uint64_t *Count
    )
{
    STATUS status = ENV_OK;
    SymbolEntry entry;
    Vector vec;
    char *line = NULL;
    size_t len = 0;
    uint64_t i;

    memset(&entry, 0, sizeof(entry));
    CHECK(status = VectorInitialize(
                    &vec,
                    NULL,
                    0,
                    sizeof(entry),
                    SymbolEntryTeardown));

    for (i = 0; ; ++i)
    {
        line = NULL;
        len = 0;
        int ret = getline(&line, &len, fp);
        if (ret < 0 && errno)
        {
            status = ENV_FAILURE;
            TagWarn("getline: %d", ret);
            goto Bail;
        }
        else if (ret <= 0)
        {
            break;
        }

        //
        // N.B., The debug format is "<symbol_name> <symbol_value>".
        //

        char *raw = strchr(line, ' ');
        if (raw == NULL)
        {
            status = ENV_FAILURE;
            TagWarnx("Malformed debug file");
            goto Bail;
        }
        *raw++ = '\0';

        errno = 0;
        uint64_t rawSymbol = strtoul(raw, NULL, 0x10);
        if (errno == ERANGE)
        {
            status = ENV_FAILURE;
            TagWarnx("Malformed debug file: raw value too large");
            goto Bail;
        }

        entry.Name = line;
        entry.RawSymbol = rawSymbol;
        CHECK(status = VectorAppend(&vec, &entry));
    }

    //
    // N.B., now the caller owns the underlying buffer and our responsible for
    // its cleanup.
    //
    *Entries = VectorBuffer(&vec);
    *Count = VectorSize(&vec);

Bail:
    if (FAILED(status))
    {
        VectorTeardown(&vec);
    }
    free(line);

    return status;
}

void
SymbolEntryTeardown(
    void *Entry
    )
{
    SymbolEntry *entry = (SymbolEntry *) Entry;

    free(entry->Name);
}

void
SymbolEntryTeardownComplete(
    void *Entry
    )
{
    SymbolEntryTeardown(Entry);

    free(Entry);
}

STATUS
DebuggerInitialize(
    Debugger *Dbg,
    SymbolEntry *Entries,
    uint64_t Size
    )
{
    STATUS status = ENV_OK;
    uint64_t i;

    tommy_hashlin_init(&Dbg->SymbolMap);

    for (i = 0; i < Size; ++i)
    {
        SymbolEntry *entry = malloc(sizeof(*entry));
        if (entry == NULL)
        {
            TagWarn("malloc entry");
            BAIL(status = ENV_OOM);
        }

        //
        // Duplicate the symbol.
        // XXX We should write a function to handle this.
        //
        entry->Name = strdup(Entries[i].Name);
        entry->RawSymbol = Entries[i].RawSymbol;

        tommy_hash_t hash = tommy_hash_u64(0,
                                &entry->RawSymbol,
                                sizeof(entry->RawSymbol));
        tommy_hashlin_insert(
                &Dbg->SymbolMap,
                &entry->Node,
                entry,
                hash);
    }

Bail:
    if (FAILED(status))
    {
        DebuggerTeardown(Dbg);
    }

    return status;
}

void
DebuggerTeardown(
    Debugger *Dbg
    )
{
    tommy_hashlin_foreach(&Dbg->SymbolMap, SymbolEntryTeardownComplete);
    tommy_hashlin_done(&Dbg->SymbolMap);
}
