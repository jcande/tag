#include <stdio.h>
#include <string.h>

#include <err.h>
#include <errno.h>
#include <unistd.h>

#include "TagRule.h"
#include "Util.h"
#include "Blob.h"

char *IoSelectorNames[IoSel_Max] = {
    "IoSel_Output", /* 0 */
    "IoSel_Input", /* 1 */
    "IoSel_Pure", /* 2 */
};


void
TagRuleTeardown(
    TagRule *Obj
    )
{
    if (Obj == NULL)
    {
        return;
    }

    BlobTeardown(&Obj->Symbol);

    switch (Obj->Style) {
    case IoSel_Pure:
        BlobTeardown(&Obj->Pure.Appendant);
        break;

    case IoSel_Input:
        BlobTeardown(&Obj->In.Appendant0);
        BlobTeardown(&Obj->In.Appendant1);
        break;

    case IoSel_Output:
        BlobTeardown(&Obj->Out.Appendant);
        break;

    default:
        //
        // Nothing to do.
        //
        break;
    }
}

//
// For *TeardownComplete, we will free the pointer to the object itself.
//
void
TagRuleTeardownComplete(
    TagRule *Obj
    )
{
    TagRuleTeardown(Obj);

    free(Obj);
}

STATUS
TagRuleDupe(
    TagRule *Source,
    TagRule **Destination
    )
{
    STATUS status = ENV_OK;
    TagRule *obj;

    obj = NULL;

    if (Source == NULL)
    {
        status = ENV_BADPTR; // invalid arguments
        TagWarnx("NULL Source");
        goto Bail;
    }
    else if (Destination == NULL)
    {
        status = ENV_BADPTR; // invalid arguments
        TagWarnx("NULL Destination");
        goto Bail;
    }

    obj = malloc(sizeof(*obj));
    if (obj == NULL)
    {
        status = ENV_OOM;
        TagWarnx("malloc obj");
        goto Bail;
    }
    memset(obj, 0, sizeof(*obj));

    //
    // Since we have no way of knowing what the proper size of a symbol is, the
    // caller must have verified this before invoking us.
    //
    status = BlobInitialize(
                &obj->Symbol,
                Source->Symbol.Data,
                Source->Symbol.Size);
    if (FAILED(status))
    {
        goto Bail;
    }

    switch (Source->Style) {
    case IoSel_Pure:
        if ((Source->Pure.Appendant.Size % Source->Symbol.Size) != 0)
        {
            status = ENV_BADLEN;
            TagWarnx("invalid Appendant size for pure rule symbol");
            goto Bail;
        }

        status = BlobInitialize(
                    &obj->Pure.Appendant,
                    Source->Pure.Appendant.Data,
                    Source->Pure.Appendant.Size);
        if (FAILED(status))
        {
            goto Bail;
        }
        break;

    case IoSel_Input:
        if ((Source->In.Appendant0.Size % Source->Symbol.Size) != 0)
        {
            status = ENV_BADLEN;
            TagWarnx("invalid Appendant0 size for in rule symbol");
            goto Bail;
        }
        if ((Source->In.Appendant1.Size % Source->Symbol.Size) != 0)
        {
            status = ENV_BADLEN;
            TagWarnx("invalid Appendant1 size for in rule symbol");
            goto Bail;
        }

        status = BlobInitialize(
                    &obj->In.Appendant0,
                    Source->In.Appendant0.Data,
                    Source->In.Appendant0.Size);
        if (FAILED(status))
        {
            goto Bail;
        }

        status = BlobInitialize(
                    &obj->In.Appendant1,
                    Source->In.Appendant1.Data,
                    Source->In.Appendant1.Size);
        if (FAILED(status))
        {
            goto Bail;
        }
        break;

    case IoSel_Output:
        if ((Source->Out.Appendant.Size % Source->Symbol.Size) != 0)
        {
            status = ENV_BADLEN;
            TagWarnx("invalid Appendant size for out rule symbol");
            goto Bail;
        }

        status = BlobInitialize(
                    &obj->Out.Appendant,
                    Source->Out.Appendant.Data,
                    Source->Out.Appendant.Size);
        if (FAILED(status))
        {
            goto Bail;
        }

        obj->Out.Bit = Source->Out.Bit;
        break;

    default:
        status = ENV_BADENUM;
        TagWarnx("Enum %x not valid", Source->Style);
        goto Bail;
    }
    obj->Style = Source->Style;

    *Destination = obj;
    obj = NULL;

Bail:
    if (obj != NULL)
    {
        TagRuleTeardown(obj);

        free(obj);
    }

    return status;
}

int
TagRuleCompare(
    const void *Arg,
    const void *Obj
    )
{
    //
    // This is the "popped" tag (and all the other data up to the deletion
    // number) from the ring buffer.
    //
    Blob *key = (Blob *) Arg;

    //
    // This is a specific rule (or production) against which the popped tag
    // will be compared.
    //
    TagRule *rule = (TagRule *) Obj;

    //
    // key is straight from the ring buffer so we must only compare the first
    // symbol's worth.
    //

    assert(key->Size >= rule->Symbol.Size);

    return memcmp(key->Data, rule->Symbol.Data, rule->Symbol.Size);
}

void
TagRuleDump(
    TagRule *Rule
    )
{
    hexdump_only("Rule Symbol", Rule->Symbol.Data, Rule->Symbol.Size);
    switch (Rule->Style) {
    case IoSel_Pure:
        hexdump_only("Pure Appendant", Rule->Pure.Appendant.Data, Rule->Pure.Appendant.Size);
        break;

    case IoSel_Input:
        hexdump_only("False Input Appendant", Rule->In.Appendant0.Data, Rule->In.Appendant0.Size);
        hexdump_only("True Input Appendant", Rule->In.Appendant0.Data, Rule->In.Appendant0.Size);
        break;

    case IoSel_Output:
        hexdump_only("Output Appendant", Rule->Out.Appendant.Data, Rule->Out.Appendant.Size);
        TagPrint("Output Bit: %d\n", Rule->Out.Bit);
        break;

    default:
        TagPrint("Invalid Style: %x\n", Rule->Style);
        break;
    }
}
