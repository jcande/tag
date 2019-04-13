#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Util.h"
#include "TagBin.h"
#include "Tag.h"
#include "TagRule.h"
#include "IoBuffer.h"
#include "Debug.h"

void
TagBinTeardown(
    TagBin *Binary
    );

STATUS
TagBinRead(
    int fd,
    void *Buffer,
    size_t Size
    )
{
    STATUS status = 0;
    ssize_t n;

    n = read(fd, Buffer, Size);
    if ((n < 0) ||
        ((size_t) n != Size))
    {
        //
        // This cast is safe as we first check to see if it was negative.
        //

        status = errno;
        if (!FAILED(status))
        {
            //
            // Unknown error occurred.
            //

            status = ENV_FAILURE;
        }
    }

    return status;
}

STATUS
ReadBinaryRules(
    int fd,
    TagBinHeader *Header,
    TagBinRule *Rules
    )
{
    STATUS status;
    uint64_t i;

    status = 0;

    for (i = 0; i < Header->RuleCount; ++i)
    {
        //
        // N.B., We rely on Rules to be zeroed. This, strictly speaking, is
        // undefined behavior because while NULL can be 0, 0 is not necessarily
        // NULL. I am ok with this shortcut.
        //

        TagBinRule *rule = &Rules[i];

        status = TagBinRead(fd, &rule->Header.Style, sizeof(rule->Header.Style));
        if (FAILED(status))
        {
            TagWarnx("read wrong size for rule header");
            goto Bail;
        }

        //
        // Zero-extend Style and then ensure we compare unsigned values.
        //
        if ((size_t) rule->Header.Style >= (size_t) IoSel_Max)
        {
            status = ENV_BADENUM;
            TagWarnx("invalid enum for rule #%lu: %x", i, rule->Header.Style);
            goto Bail;
        }

        switch (rule->Header.Style) {
        case IoSel_Pure:
            status = TagBinRead(fd,
                        &rule->Header.Pure,
                        sizeof(rule->Header.Pure));
            if (FAILED(status))
            {
                TagWarnx("read wrong size for rule property header");
                goto Bail;
            }

            if ((rule->Header.Pure.AppendantSize % Header->SymbolSize) != 0)
            {
                status = ENV_BADLEN;
                TagWarnx("invalid AppendantSize for rule symbol #%lu", i);
                goto Bail;
            }
            break;

        case IoSel_Input:
            status = TagBinRead(fd,
                        &rule->Header.In,
                        sizeof(rule->Header.In));
            if (FAILED(status))
            {
                TagWarnx("read wrong size for rule property header");
                goto Bail;
            }

            if ((rule->Header.In.Appendant0Size % Header->SymbolSize) != 0)
            {
                status = ENV_BADLEN;
                TagWarnx("invalid Appendant0Size for rule symbol #%lu", i);
                goto Bail;
            }
            if ((rule->Header.In.Appendant1Size % Header->SymbolSize) != 0)
            {
                status = ENV_BADLEN;
                TagWarnx("invalid Appendant1Size for rule symbol #%lu", i);
                goto Bail;
            }
            break;

        case IoSel_Output:
            status = TagBinRead(fd,
                        &rule->Header.Out,
                        sizeof(rule->Header.Out));
            if (FAILED(status))
            {
                TagWarnx("read wrong size for rule property header");
                goto Bail;
            }

            if ((rule->Header.Out.AppendantSize % Header->SymbolSize) != 0)
            {
                status = ENV_BADLEN;
                TagWarnx("invalid AppendantSize for rule symbol #%lu", i);
                goto Bail;
            }
            break;

        default:
            assert(false);
            status = ENV_BADENUM;
            TagWarnx("invalid enum for rule #%lu", i);
            goto Bail;
        }

        rule->RawSymbol = malloc(Header->SymbolSize);
        if (rule->RawSymbol == NULL)
        {
            status = ENV_OOM;
            TagWarnx("malloc rule->RawSymbol");
            goto Bail;
        }

        status = TagBinRead(fd, rule->RawSymbol, Header->SymbolSize);
        if (FAILED(status))
        {
            TagWarnx("read wrong size for rule symbol #%lu", i);
            goto Bail;
        }


        switch (rule->Header.Style) {
        case IoSel_Pure:
            if (rule->Header.Pure.AppendantSize != 0)
            {
                rule->Pure.RawAppendant = malloc(rule->Header.Pure.AppendantSize);
                if (rule->Pure.RawAppendant == NULL)
                {
                    status = ENV_OOM;
                    TagWarnx("malloc rule->Pure.RawAppendant");
                    goto Bail;
                }
            }

            status = TagBinRead(fd,
                        rule->Pure.RawAppendant,
                        rule->Header.Pure.AppendantSize);
            if (FAILED(status))
            {
                TagWarnx("read wrong size for pure rule RawAppendant #%lu", i);
                goto Bail;
            }
            break;

        case IoSel_Input:
            if (rule->Header.In.Appendant0Size != 0)
            {
                rule->In.RawAppendant0 = malloc(rule->Header.In.Appendant0Size);
                if (rule->In.RawAppendant0 == NULL)
                {
                    status = ENV_OOM;
                    TagWarnx("malloc rule->In.RawAppendant0");
                    goto Bail;
                }
            }
            if (rule->Header.In.Appendant1Size != 0)
            {
                rule->In.RawAppendant1 = malloc(rule->Header.In.Appendant1Size);
                if (rule->In.RawAppendant1 == NULL)
                {
                    status = ENV_OOM;
                    TagWarnx("malloc rule->In.RawAppendant1");
                    goto Bail;
                }
            }

            status = TagBinRead(fd,
                        rule->In.RawAppendant0,
                        rule->Header.In.Appendant0Size);
            if (FAILED(status))
            {
                TagWarnx("read wrong size for in rule RawAppendant0 #%lu", i);
                goto Bail;
            }
            status = TagBinRead(fd,
                        rule->In.RawAppendant1,
                        rule->Header.In.Appendant1Size);
            if (FAILED(status))
            {
                TagWarnx("read wrong size for in rule RawAppendant1 #%lu", i);
                goto Bail;
            }
            break;

        case IoSel_Output:
            if (rule->Header.Out.AppendantSize != 0)
            {
                rule->Out.RawAppendant = malloc(rule->Header.Out.AppendantSize);
                if (rule->Out.RawAppendant == NULL)
                {
                    status = ENV_OOM;
                    TagWarnx("malloc rule->Out.RawAppendant");
                    goto Bail;
                }
            }

            status = TagBinRead(fd,
                        rule->Out.RawAppendant,
                        rule->Header.Out.AppendantSize);
            if (FAILED(status))
            {
                TagWarnx("read wrong size for out rule RawAppendant #%lu", i);
                goto Bail;
            }

            status = TagBinRead(fd,
                        &rule->Out.Bit,
                        sizeof(rule->Out.Bit));
            if (FAILED(status))
            {
                TagWarnx("read wrong size for out rule Bit #%lu", i);
                goto Bail;
            }
            break;

        default:
            assert(false);
            status = ENV_BADENUM;
            TagWarnx("invalid enum for rule #%lu: %x", i, rule->Header.Style);
            goto Bail;
        }
    }

Bail:

    //
    // N.B., do not cleanup here as this is merely a helper method. The caller
    // will deal with this properly.
    //

    return status;
}

STATUS
ReadBinaryFile(
    int fd,
    TagBin *Binary
    )
{
    STATUS status;

    status = 0;

    Binary->Rules = NULL;
    Binary->Queue = NULL;

    status = TagBinRead(fd, &Binary->Header, sizeof(Binary->Header));
    if (FAILED(status))
    {
        TagWarnx("Read wrong size for header");
        goto Bail;
    }

    if ((Binary->Header.QueueSize % Binary->Header.SymbolSize) != 0)
    {
        status = ENV_BADLEN;
        TagWarnx("invalid QueueSize");
        goto Bail;
    }

    if (Binary->Header.DeletionNumber < 2)
    {
        status = TAGSS_BADDELETIONNUMBER;
        TagWarnx("invalid deletion number: %u", Binary->Header.DeletionNumber);
        goto Bail;
    }

    //
    // N.B., We rely on the fact that calloc zeroes the data before handing it
    // back to us. This means that if an error occurs we will interpret the
    // zeroed data as NULL pointers and can safely call free on them. This
    // simplifies the amount of bookkeeping necessary.
    // XXX This is not stricly compliant as NULL is *not* guaranteed to have
    // the representation of all zeroes. Man, C sucks.
    //

    Binary->Rules = calloc(Binary->Header.RuleCount, sizeof(*Binary->Rules));
    if (Binary->Rules == NULL)
    {
        status = ENV_OOM;
        TagWarn("calloc Rules");
        goto Bail;
    }

    Binary->Queue = malloc(Binary->Header.QueueSize);
    if (Binary->Queue == NULL)
    {
        status = ENV_OOM;
        TagWarn("malloc Queue");
        goto Bail;
    }

    status = ReadBinaryRules(fd, &Binary->Header, Binary->Rules);
    if (FAILED(status))
    {
        goto Bail;
    }

    status = TagBinRead(fd, Binary->Queue, Binary->Header.QueueSize);
    if (FAILED(status))
    {
        TagWarnx("read wrong size for queue");
        goto Bail;
    }

Bail:
    if (FAILED(status))
    {
#if 1 /* XXX this is commented only for dumping! */
        TagBinTeardown(Binary);
#endif
    }

    return status;
}

void
TagBinTeardown(
    TagBin *Binary
    )
{
    uint64_t i;

    if (Binary->Rules)
    {
        for (i = 0; i < Binary->Header.RuleCount; ++i)
        {
            free(Binary->Rules[i].RawSymbol);

            switch (Binary->Rules[i].Header.Style) {
            case IoSel_Pure:
                free(Binary->Rules[i].Pure.RawAppendant);
                break;

            case IoSel_Input:
                free(Binary->Rules[i].In.RawAppendant0);
                free(Binary->Rules[i].In.RawAppendant1);
                break;

            case IoSel_Output:
                free(Binary->Rules[i].Out.RawAppendant);
                break;

            default:
                assert(false);
                break;
            }
        }
        free(Binary->Rules);
        Binary->Rules = NULL;
    }

    free(Binary->Queue);
    Binary->Queue = NULL;
}

void
TagBinDump(
    TagBin *Binary
    )
{
    uint64_t i, level = 0;

    TagPrint("Header:\n");
    ++level;
    {
        TagPrint("RuleCount: %lu\n", Binary->Header.RuleCount);
        TagPrint("SymbolSize: %u\n", Binary->Header.SymbolSize);
        TagPrint("QueueSize: %u\n", Binary->Header.QueueSize);
        TagPrint("DeletionNumber: %u\n", Binary->Header.DeletionNumber);

        TagPrint("\n");

        TagPrint("Rules:\n");
        ++level;
        {
            for (i = 0; i < Binary->Header.RuleCount; ++i) {
                TagBinRule *rule = &Binary->Rules[i];

                TagPrint("Rule #%lu Header:\n", i);
                ++level;
                {
                    char *name = "Invalid";
                    if (rule->Header.Style < IoSel_Max)
                    {
                        name = IoSelectorNames[rule->Header.Style];
                    }
                    TagPrint("Style: %x (%s)\n", rule->Header.Style, name);

                    switch (rule->Header.Style) {
                    case IoSel_Pure:
                        TagPrint("Appendant Length: %x\n", rule->Header.Pure.AppendantSize);
                        TagPrint("\n");
                        break;

                    case IoSel_Input:
                        TagPrint("Appendant0 Length: %x\n", rule->Header.In.Appendant0Size);
                        TagPrint("Appendant1 Length: %x\n", rule->Header.In.Appendant1Size);
                        TagPrint("\n");
                        break;

                    case IoSel_Output:
                        TagPrint("Appendant Length: %x\n", rule->Header.Out.AppendantSize);
                        TagPrint("\n");
                        break;

                    default:
                        hexdump_only("Invalid Header", &rule->Header, sizeof(rule->Header));
                        TagPrint("\n");
                        break;
                    }
                    TagPrint("\n");

                    // XXX we don't validate the shit at all :(
                    hexdump_only("Raw symbol", rule->RawSymbol, Binary->Header.SymbolSize);
                    TagPrint("\n");

                    switch (rule->Header.Style) {
                    case IoSel_Pure:
                        // XXX we don't validate the shit at all :(
                        hexdump_only("Appendant", rule->Pure.RawAppendant, rule->Header.Pure.AppendantSize);
                        break;

                    case IoSel_Input:
                        // XXX we don't validate the shit at all :(
                        hexdump_only("False Appendant", rule->In.RawAppendant0, rule->Header.In.Appendant0Size);
                        hexdump_only("True Appendant", rule->In.RawAppendant1, rule->Header.In.Appendant1Size);
                        break;

                    case IoSel_Output:
                        // XXX we don't validate the shit at all :(
                        hexdump_only("Appendant", rule->Out.RawAppendant, rule->Header.Out.AppendantSize);
                        TagPrint("Bit: %s\n", rule->Out.Bit ? "True" : "False");
                        break;

                    default:
                        //hexdump_only("Invalid Body", &rule->Header, sizeof(rule->Header));
                        break;
                    }
                    TagPrint("\n");

                }
                --level;
            }
        }
        --level;
    }
    --level;
}

STATUS
InstantiateTagSystemFromBinary(
    TagBin *Binary,
    TagSystem *System,
    IoBufferConfig *Io
    )
{
    TagRule *rules;
    Blob *tape;
    uint64_t i;
    STATUS status = 0;

    rules = NULL;
    tape = NULL;


    //
    // Allocate space to store the rules. We use the fact that calloc zeroes
    // this memory.
    //

    rules = calloc(Binary->Header.RuleCount, sizeof(*rules));
    if (rules == NULL)
    {
        status = ENV_OOM;
        TagWarnx("calloc rules");
        goto Bail;
    }


    //
    // Allocate space to store the tape.
    //

    tape = malloc(sizeof(*tape));
    if (tape == NULL)
    {
        status = ENV_OOM;
        TagWarnx("malloc tape");
        goto Bail;
    }


    //
    // Copy over the rules.
    //

    for (i = 0; i < Binary->Header.RuleCount; ++i)
    {
        // XXX this should be converted into TagRuleInitialization (1 per tag rule style)
        TagBinRule *binRule = &Binary->Rules[i];
        TagRule *rule = &rules[i];

        status = BlobInitialize(
                    &rule->Symbol,
                    binRule->RawSymbol,
                    Binary->Header.SymbolSize);
        if (FAILED(status))
        {
            TagWarnx("BlobInitialize rule->Symbol");
            goto Bail;
        }

        rule->Style = binRule->Header.Style;
        switch (rule->Style) {
        case IoSel_Pure:
            status = BlobInitialize(
                        &rule->Pure.Appendant,
                        binRule->Pure.RawAppendant,
                        binRule->Header.Pure.AppendantSize);
            if (FAILED(status))
            {
                TagWarnx("BlobInitialize rule->Pure.Appendant");
                goto Bail;
            }
            break;

        case IoSel_Input:
            status = BlobInitialize(
                        &rule->In.Appendant0,
                        binRule->In.RawAppendant0,
                        binRule->Header.In.Appendant0Size);
            if (FAILED(status))
            {
                TagWarnx("BlobInitialize rule->In.Appendant0");
                goto Bail;
            }

            status = BlobInitialize(
                        &rule->In.Appendant1,
                        binRule->In.RawAppendant1,
                        binRule->Header.In.Appendant1Size);
            if (FAILED(status))
            {
                TagWarnx("BlobInitialize rule->In.Appendant1");
                goto Bail;
            }

            break;

        case IoSel_Output:
            status = BlobInitialize(
                        &rule->Out.Appendant,
                        binRule->Out.RawAppendant,
                        binRule->Header.Out.AppendantSize);
            if (FAILED(status))
            {
                TagWarnx("BlobInitialize rule->Out.Appendant");
                goto Bail;
            }

            rule->Out.Bit = (binRule->Out.Bit != 0);
            break;

        default:
            status = ENV_BADENUM;
            goto Bail;
        }
    }


    //
    // Copy the tape.
    //

    status = BlobInitialize(
                tape,
                Binary->Queue,
                Binary->Header.QueueSize);
    if (FAILED(status))
    {
        TagWarnx("BlobInitialize queue");
        goto Bail;
    }


    //
    // Finally, initialize the tag system.
    //

    status = TagInitialize(
                System,
                rules,
                Binary->Header.RuleCount,
                tape,
                Binary->Header.DeletionNumber,
                Io);
    if (FAILED(status))
    {
        TagWarnx("Unable to initialize tag system");
        goto Bail;
    }

Bail:

    if (rules != NULL)
    {
        for (i = 0; i < Binary->Header.RuleCount; ++i)
        {
            TagRuleTeardown(&rules[i]);
        }
        free(rules);
    }

    if (tape != NULL)
    {
        if (tape->Data != NULL)
        {
            BlobTeardown(tape);
        }

        free(tape);
    }

    return status;
}

static
STATUS
PrepareDebugger(
    Debugger *Dbg,
    char *SymbolsFilename,
    TagSystem *System
    )
{
    STATUS status = ENV_OK;
    SymbolEntry *entries = NULL;
    uint64_t count = 0;
    uint64_t i = 0;
    FILE *dbg_fp = NULL;

    (void) System;

    dbg_fp = fopen(SymbolsFilename, "r");
    if (dbg_fp == NULL)
    {
        TagWarn("open (dbg_fp)");
        goto Bail;
    }

    CHECK(status = ReadSymbolFile(dbg_fp, &entries, &count));

    CHECK(status = DebuggerInitialize(Dbg, entries, count));

Bail:
    for (i = 0; i < count; ++i)
    {
        SymbolEntryTeardown(&entries[i]);
    }
    free(entries);

    if (dbg_fp != NULL)
    {
        (void) fclose(dbg_fp);
    }

    return status;
}

void
Usage(
    void
    )
{
    TagPrint("usage\n");

    exit(1);
}

// XXX Make a real main.c and have this just be a switch to reach something similar to this
int
main(
    int argc,
    char **argv
    )
{
    TagBin binary;
    TagSystem system;
    IoBufferConfig io;
    Debugger dbg;

    bool binary_initialized;
    bool system_initialized;
    bool debugger_initialized;
    int ch;

    binary_initialized = false;
    system_initialized = false;
    debugger_initialized = false;

    char *filename = NULL;
    int fd = STDIN_FILENO;

    char *debug_file = NULL;

    int print = 0;

    STATUS status = 0;

    while ((ch = getopt(argc, argv, "d:f:p")) != -1)
    {
        switch (ch) {
        case 'd':
            printf("optarg: %s\n", optarg);
            debug_file = optarg;
            break;

        case 'f':
            filename = optarg;
            break;

        case 'p':
            // XXX make this a log-level (or even subsystem) so we can hook up
            // a proper logger to print out various info without having to just
            // comment it out
            print = 1;
            break;

        // XXX maybe have a "steps" argument so we can run for <x> steps
        // this is part of the debugger

        // TODO implement a jit :)

        default:
            Usage();
            break;
        }
    }
    argc -= optind;
    argv += optind;

    if (filename != NULL)
    {
        fd = open(filename, O_RDONLY);
        if (fd < 0)
        {
            TagWarn("open (fd)");
            goto Bail;
        }
    }

    if (!print)
    {
        io.GetByte = (GetByteFn) getc;
        io.GetContext = stdin;
        io.PutByte = (PutByteFn) putc;
        io.PutContext = stdout;

        status = ReadBinaryFile(fd, &binary);
        if (FAILED(status))
        {
            goto Bail;
        }
        binary_initialized = true;

        status = InstantiateTagSystemFromBinary(&binary, &system, &io);
        if (FAILED(status))
        {
            goto Bail;
        }
        system_initialized = true;

        if (debug_file != NULL)
        {
            CHECK(status = PrepareDebugger(&dbg, debug_file, &system));
            debugger_initialized = true;
        }

        hexdump_only("Starting Queue", system.InitialQueue.Data, system.InitialQueue.Size);

        uint64_t steps;
        for (steps = 0; ; ++steps)
        {
            status = TagStep(&system);
            if (FAILED(status))
            {
                TagWarnx("TagStep: %x", status);

                TagPrint("steps: %lx\n", steps);

                break;
            }

            /*
            TagDump(&system);
            TagPrint("\n");
            */
        }
    }
    else
    {
        binary_initialized = true;
        (void) ReadBinaryFile(fd, &binary);
        TagBinDump(&binary);
    }

Bail:
    if (fd > 0 && fd != STDIN_FILENO)
    {
        (void) close(fd);
    }

    if (binary_initialized)
    {
        TagBinTeardown(&binary);
    }
    if (system_initialized)
    {
        TagTeardown(&system);
    }
    if (debugger_initialized)
    {
        DebuggerTeardown(&dbg);
    }

    return (status < 0);
}
