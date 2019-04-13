#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <err.h>
#include <errno.h>
#include <unistd.h>

#include "tommyhashlin.h"

#include "Util.h"
#include "Blob.h"
#include "IoBuffer.h"
#include "TagRule.h"
#include "Tag.h"

// TODO translate a normal tag system into a cyclic tag system

// XXX everything passed to an instance of tommy_hashtable LOSES ownership.
// That means the hashtable is responsible for cleaning everything up.


STATUS
TagInitialize(
    TagSystem *System,
    TagRule *Rules,
    uint64_t RulesCount,
    Blob *DefaultQueue,
    uint32_t AbstractDeletionNumber,
    IoBufferConfig *Io
    )
{
    STATUS status = ENV_OK;
    uint64_t i;

    assert(System != NULL);
    assert(Rules != NULL);

    memset(System, 0, sizeof(*System));

    tommy_hashlin_init(&System->Productions);

    if (RulesCount < 1)
    {
        status = TAGSS_BADRULECOUNT;
        TagWarnx("Must have at least 1 rule");
        goto Bail;
    }

    if (AbstractDeletionNumber < 2)
    {
        status = TAGSS_BADDELETIONNUMBER;
        TagWarnx("Deletion number must be at least 2");
        goto Bail;
    }

    //
    // Since all symbols must be the same length we can choose any symbol's
    // length. For convenience we choose the first one and then verify the rest
    // match as we iterate over them.
    //
    assert(Rules->Symbol.Size == (uint32_t) Rules->Symbol.Size);
    System->AbstractDeletionNumber = AbstractDeletionNumber;
    System->SymbolSize = Rules->Symbol.Size;

    CHECK(status = IoBufferInit(
                &System->Io,
                Io));

    //
    // Take ownership of the starting data.
    //
    CHECK(status = BlobCopy(&System->InitialQueue, DefaultQueue));

    CHECK(status = TagQueueInitialize(
            &System->Tape,
            AbstractDeletionNumber,
            System->SymbolSize));
    CHECK(status = TagQueuePush(
        &System->Tape,
        &System->InitialQueue,
        1));


    //
    // We now *own* these rules and are members are free to reference these
    // (read-only). We are the last to teardown and we destroy the rules when
    // we do.
    //

    for (i = 0; i < RulesCount; ++i)
    {
        TagRule *rule;
        tommy_hash_t hash;

        if (Rules[i].Symbol.Size != System->SymbolSize)
        {
            status = ENV_BADLEN;
            TagWarnx("Rule #%lu's start symbol size (%lu) differs from the stated symbol size of %u", i, Rules[i].Symbol.Size, System->SymbolSize);
            goto Bail;
        }

        status = TagRuleDupe(&Rules[i], &rule);
        if (FAILED(status))
        {
            goto Bail;
        }

        hash = tommy_hash_u64(0, rule->Symbol.Data, rule->Symbol.Size);
        if (tommy_hashlin_search(
                &System->Productions,
                TagRuleCompare,
                &rule->Symbol,
                hash) != NULL)
        {
            status = TAGSS_DUPERULE;
            TagWarnx("Rule #%lu's already been added", i);
            goto Bail;
        }
        tommy_hashlin_insert(
                &System->Productions,
                &rule->Node,
                rule,
                hash);
    }

Bail:
    if (FAILED(status))
    {
        TagTeardown(System);
    }

    return status;
}

void
TagTeardown(
    TagSystem *System
    )
{
    tommy_hashlin_foreach(
        &System->Productions,
        (tommy_foreach_func *) TagRuleTeardownComplete);

    tommy_hashlin_done(&System->Productions);

    TagQueueTeardown(&System->Tape);

    // XXX we are using "teardown" inconsistently and in this case it doesn't
    // free everything (i.e., system). Is this ok?
}

STATUS
TagStep(
    TagSystem *System
    )
{
    STATUS status = ENV_OK;
    TagRule *rule;
    Blob deleted, appendant;
    uint64_t reps;
    bool input;
    tommy_hash_t hash;

    rule = NULL;
    memset(&appendant, 0, sizeof(appendant));

    status = TagQueuePop(
                &System->Tape,
                &deleted,
                &reps);
    if (FAILED(status))
    {
        status = TAGSS_OUTOFTAPE;
        TagWarnx("TagQueuePop: Queue empty?");
        goto Bail;
    }
    assert(deleted.Size == System->SymbolSize);
    assert(reps > 0);

    // XXX We should implement a shitty debugger.
    hexdump_only("popped symbol", deleted.Data, System->SymbolSize);

    hash = tommy_hash_u64(0, deleted.Data, System->SymbolSize);
    rule = (TagRule *) tommy_hashlin_search(
                        &System->Productions,
                        TagRuleCompare,
                        &deleted,
                        hash);
    if (rule == NULL)
    {
        status = TAGSS_BADRULE;
        TagWarnx("tommy_hashtable_search: Invalid address");
        goto Bail;
    }

    assert(rule->Style >= IoSel_Min && rule->Style < IoSel_Max);

    //TagRuleDump(rule);

    switch (rule->Style) {
    case IoSel_Pure:
        appendant = rule->Pure.Appendant;
        break;

    case IoSel_Input:
        status = IoBufferGetBit(&System->Io, &input);
        if (FAILED(status))
        {
            goto Bail;
        }

        if (input)
        {
            appendant = rule->In.Appendant1;
        }
        else
        {
            appendant = rule->In.Appendant0;
        }
        break;

    case IoSel_Output:
        status = IoBufferPutBit(&System->Io, rule->Out.Bit);
        if (FAILED(status))
        {
            goto Bail;
        }

        appendant = rule->Out.Appendant;

        break;

    case IoSel_Max:
    default:
        assert(false);

        status = ENV_FAILURE;
        warn("TagStep: Malformed rule with invalid enum %x", rule->Style);
        goto Bail;
    }

    if (appendant.Size == 0)
    {
        status = TAGSS_HALT;
        goto Bail;
    }

    TagPrint("Rep: %lu\n", reps);
    hexdump_only("pushed appendant", appendant.Data, appendant.Size);
    TagPrint("\n");

    status = TagQueuePush(
                &System->Tape,
                &appendant,
                reps);
    if (FAILED(status))
    {
        goto Bail;
    }

Bail:
    return status;
}

STATUS
TagRun(
    TagSystem *System
 /*   Debugger *Dbg */
    )
{
    STATUS status = ENV_OK;

    (void) System;

    goto Bail;

Bail:
    return status;
}

void
TagDump(
    TagSystem *System
    )
{
    // XXX Instead of malloc'ing every call, we should have the temporary
    // buffer passed in to us. We could make Deletion as big as the largest
    // appendent and then we could use that as our backing buffer.
    uint8_t *data = NULL;

#if 0 // XXX gotta update for TagQueue
    if (System->Tape.ActiveSize > 0)
    {
        data = malloc(System->Tape.ActiveSize);
        if (data == NULL)
        {
            TagWarnx("malloc");
            goto Bail;
        }

        if (FAILED(RingBufferPeek(&System->Tape, data, System->Tape.ActiveSize)))
        {
            TagWarnx("RingBufferPeek");
            goto Bail;
        }

        hexdump_only("Queue", data, System->Tape.ActiveSize);
    }

Bail:
#else
    (void) System;
#endif
    free(data);
}

/*
static
STATUS
TestTag(
    void
    )
{
    STATUS status = 0;
    TagSystem system;

    uint8_t match0[] = { 'a' };
    //uint8_t appendant0[] = { 'c', 'c', 'b', 'a', 'H' };
    uint8_t appendant0[] = { 'b', 'c' };

    uint8_t match1[] = { 'b' };
    //uint8_t appendant1[] = { 'c', 'c', 'a' };
    uint8_t appendant1[] = { 'a' };

    uint8_t match2[] = { 'c' };
    //uint8_t appendant2[] = { 'c', 'c' };
    uint8_t appendant2[] = { 'a', 'a', 'a' };

    uint8_t match3[] = { 'H' }; // halting rule
    TagRule rules[4];
    //uint8_t tapeData[] = { 'b', 'a', 'a' };
    uint8_t tapeData[] = { 'a', 'a', 'a' };
    Blob tape;

    (void) BlobInitialize(
                &rules[0].Key,
                match0,
                sizeof(match0));
    (void) BlobInitialize(
                &rules[0].Value,
                appendant0,
                sizeof(appendant0));

    (void) BlobInitialize(
                &rules[1].Key,
                match1,
                sizeof(match1));
    (void) BlobInitialize(
                &rules[1].Value,
                appendant1,
                sizeof(appendant1));

    (void) BlobInitialize(
                &rules[2].Key,
                match2,
                sizeof(match2));
    (void) BlobInitialize(
                &rules[2].Value,
                appendant2,
                sizeof(appendant2));

    (void) BlobInitialize(
                &rules[3].Key,
                match3,
                sizeof(match3));
    (void) BlobInitialize(
                &rules[3].Value,
                NULL,
                0);

    (void) BlobInitialize(
                &tape,
                tapeData,
                sizeof(tapeData));

    status = TagInitialize(
                &system,
                &rules[0],
                ARRAY_SIZE(rules),
                &tape,
                2);
    if (FAILED(status))
    {
        TagWarnx("Unable to initialize tag system");
        goto Bail;
    }

    uint64_t steps;
    for (steps = 0; ; ++steps)
    {
        status = TagStep(&system);
        if (FAILED(status))
        {
            TagWarnx("TagStep");
            RingBufferDump(&system.Tape);
            printf("steps: %lx\n", steps);
            break;
        }

        TagDump(&system);
    }

Bail:
    return FAILED(status);
}

int
main(void)
{
    return TestTag();
}
*/
