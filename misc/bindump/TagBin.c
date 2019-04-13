#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include <err.h>
#include <errno.h>
#include <unistd.h>

#include "Util.h"
#include "TagBin.h"
#include "Tag.h"

void
TagBinTeardown(
    TagBin *Binary
    );

int
TagBinRead(
    int fd,
    void *Buffer,
    size_t Size
    )
{
    int status = 0;
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

int
ReadBinaryRules(
    int fd,
    TagBinHeader *Header,
    TagBinRule *Rules
    )
{
    int status;
    uint64_t i;

    status = 0;

    for (i = 0; i < Header->RuleCount; ++i)
    {
        //
        // N.B., We rely on Rules to be zeroed.
        //

        TagBinRule *rule = &Rules[i];

        status = TagBinRead(fd, &rule->Header.Style, sizeof(rule->Header.Style));
        if (FAILED(status))
        {
            warnx("ReadBinaryRules: read wrong size for rule header");
            goto Bail;
        }

        //
        // Zero-extend Style and then ensure we compare unsigned values.
        //
        if ((size_t) rule->Header.Style >= (size_t) IoSel_Max)
        {
            status = ENV_BADENUM;
            warnx("ReadBinaryRules: invalid enum for rule #%lu", i);
            goto Bail;
        }

        switch (rule->Header.Style) {
        case IoSel_Pure:
            status = TagBinRead(fd,
                        &rule->Header.Pure,
                        sizeof(rule->Header.Pure));
            break;

        case IoSel_Input:
            status = TagBinRead(fd,
                        &rule->Header.In,
                        sizeof(rule->Header.In));
            break;

        case IoSel_Output:
            status = TagBinRead(fd,
                        &rule->Header.Out,
                        sizeof(rule->Header.Out));
            break;

        default:
            assert(false);
            status = ENV_BADENUM;
            warnx("ReadBinaryRules: invalid enum for rule #%lu", i);
            goto Bail;
        }
        if (FAILED(status))
        {
            warnx("ReadBinaryRules: read wrong size for rule property header");
            goto Bail;
        }

        switch (rule->Header.Style) {
        case IoSel_Pure:
            if ((rule->Header.Pure.AppendantSize % Header->SymbolSize) != 0)
            {
                status = ENV_BADLEN;
                warnx("ReadBinaryRules: invalid AppendantSize for rule symbol #%lu", i);
                goto Bail;
            }
            break;

        case IoSel_Input:
            if ((rule->Header.In.Appendant0Size % Header->SymbolSize) != 0)
            {
                status = ENV_BADLEN;
                warnx("ReadBinaryRules: invalid Appendant0Size for rule symbol #%lu", i);
                goto Bail;
            }
            if ((rule->Header.In.Appendant1Size % Header->SymbolSize) != 0)
            {
                status = ENV_BADLEN;
                warnx("ReadBinaryRules: invalid Appendant1Size for rule symbol #%lu", i);
                goto Bail;
            }
            break;

        case IoSel_Output:
            if ((rule->Header.Out.AppendantSize % Header->SymbolSize) != 0)
            {
                status = ENV_BADLEN;
                warnx("ReadBinaryRules: invalid AppendantSize for rule symbol #%lu", i);
                goto Bail;
            }
            break;

        default:
            assert(false);
            status = ENV_BADENUM;
            warnx("ReadBinaryRules: invalid enum for rule #%lu", i);
            goto Bail;
        }

        rule->RawSymbol = malloc(Header->SymbolSize);
        if (rule->RawSymbol == NULL)
        {
            status = ENV_OOM;
            warnx("ReadBinaryRules: malloc rule->RawSymbol");
            goto Bail;
        }

        status = TagBinRead(fd, rule->RawSymbol, Header->SymbolSize);
        if (FAILED(status))
        {
            warnx("ReadBinaryRules: read wrong size for rule symbol #%lu", i);
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
                    warnx("ReadBinaryRules: malloc rule->Pure.RawAppendant");
                    goto Bail;
                }
            }

            status = TagBinRead(fd,
                        rule->Pure.RawAppendant,
                        rule->Header.Pure.AppendantSize);
            if (FAILED(status))
            {
                warnx("ReadBinaryRules: read wrong size for pure rule RawAppendant #%lu", i);
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
                    warnx("ReadBinaryRules: malloc rule->In.RawAppendant0");
                    goto Bail;
                }
            }
            if (rule->Header.In.Appendant1Size != 0)
            {
                rule->In.RawAppendant1 = malloc(rule->Header.In.Appendant1Size);
                if (rule->In.RawAppendant1 == NULL)
                {
                    status = ENV_OOM;
                    warnx("ReadBinaryRules: malloc rule->In.RawAppendant1");
                    goto Bail;
                }
            }

            status = TagBinRead(fd,
                        rule->In.RawAppendant0,
                        rule->Header.In.Appendant0Size);
            if (FAILED(status))
            {
                warnx("ReadBinaryRules: read wrong size for in rule RawAppendant0 #%lu", i);
                goto Bail;
            }
            status = TagBinRead(fd,
                        rule->In.RawAppendant1,
                        rule->Header.In.Appendant1Size);
            if (FAILED(status))
            {
                warnx("ReadBinaryRules: read wrong size for in rule RawAppendant1 #%lu", i);
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
                    warnx("ReadBinaryRules: malloc rule->Out.RawAppendant");
                    goto Bail;
                }
            }

            status = TagBinRead(fd,
                        rule->Out.RawAppendant,
                        rule->Header.Out.AppendantSize);
            if (FAILED(status))
            {
                warnx("ReadBinaryRules: read wrong size for out rule RawAppendant #%lu", i);
                goto Bail;
            }

            status = TagBinRead(fd,
                        &rule->Out.Bit,
                        sizeof(rule->Out.Bit));
            if (FAILED(status))
            {
                warnx("ReadBinaryRules: read wrong size for out rule Bit #%lu", i);
                goto Bail;
            }
            break;

        default:
            assert(false);
            status = ENV_BADENUM;
            warnx("ReadBinaryRules: invalid enum for rule #%lu", i);
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

int
ReadBinaryFile(
    int fd,
    TagBin *Binary
    )
{
    int status;

    status = 0;

    Binary->Rules = NULL;
    Binary->Queue = NULL;

    status = TagBinRead(fd, &Binary->Header, sizeof(Binary->Header));
    if (FAILED(status))
    {
        warnx("ReadBinaryFile: Read wrong size for header");
        goto Bail;
    }

    if ((Binary->Header.QueueSize % Binary->Header.SymbolSize) != 0)
    {
        status = ENV_BADLEN;
        warnx("ReadBinaryFile: invalid QueueSize");
        goto Bail;
    }

    if (Binary->Header.DeletionNumber < 2)
    {
        status = TAGSS_BADDELETIONNUMBER;
        warn("ReadBinaryFile: invalid deletion number: %u", Binary->Header.DeletionNumber);
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
        warn("ReadBinaryFile: calloc Rules");
        goto Bail;
    }

    Binary->Queue = malloc(Binary->Header.QueueSize);
    if (Binary->Queue == NULL)
    {
        status = ENV_OOM;
        warn("ReadBinaryFile: malloc Queue");
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
        warnx("ReadBinaryFile: read wrong size for queue");
        goto Bail;
    }

Bail:

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
TagBinTeardown(
    TagBin *Binary
    )
{
    uint64_t i, level = 0;

    printf("Header:\n");
    {
        ++level;
        printf("RuleCount: %lu\n", Binary->Header.RuleCount);
        printf("SymbolSize: %u\n", Binary->Header.SymbolSize);
        printf("QueueSize: %u\n", Binary->Header.QueueSize);
        printf("DeletionNumber: %u\n", Binary->Header.DeletionNumber);

        printf("\n");

        printf("Rules:\n");
        {
            ++level;
            for (i = 0; i < Binary->Header.RuleCount; ++i) {
                TagBinRule *rule = &Binary->Rules[i];

                printf("Rule #%lu Header:\n");
                ++level;
                {
                    char *Names[] = {
                        "Output",
                        "Input",
                        "Pure"
                    };
                    char *name = "Invalid";
                    /*
                    if (rule->Header.Style < IoSel_Max)
                    {
                        name = 
                    }
                    */
                    if (rule->Header.Style < ARRAY_SIZE(Names))
                    {
                        name = 
                    }
                    printf("Style: %x (%s)\n", 
                }
                --level;
            }
}

int
main(
    void
    )
{
    TagBin binary;
    int binary_initialized;
    int status;

    binary_initialized = 0;

    status = ReadBinaryFile(STDIN_FILENO, &binary);

    TagBinDump(&binary);


Bail:
    TagBinTeardown(&binary);

    return (status < 0);
}
