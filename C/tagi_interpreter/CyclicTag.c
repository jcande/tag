#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <err.h>
#include <errno.h>
#include <unistd.h>

#include "RingBuffer.h"
#include "Blob.h"
#include "Util.h"
#include "CyclicTag.h"

int
CyclicTagInitialize(
    CyclicTag *Config,
    Blob **Appendants,
    uint64_t AppendantsSize,
    uint64_t MaxTape
    )
{
    uint64_t blobSize;
    int status = 0;

    memset(Config, 0, sizeof(*Config));

    status = RingBufferInitialize(&Config->Tape, MaxTape, RingBuffer_Expandable);
    if (FAILED(status))
    {
        goto Bail;
    }

    if (__builtin_mul_overflow(AppendantsSize, sizeof(void *), &blobSize))
    {
        status = ENV_INT_OVERFLOW;
        goto Bail;
    }

    Config->Appendants = malloc(blobSize);
    if (Config->Appendants == NULL)
    {
        status = ENV_OOM;
        goto Bail;
    }
    
    for (Config->AppendantsSize = 0;
         Config->AppendantsSize < AppendantsSize;
         ++Config->AppendantsSize)
    {
        uint64_t i = Config->AppendantsSize;

        status = BlobDupe(
            Appendants[i],
            &Config->Appendants[i]
            );
        if (FAILED(status))
        {
            status = ENV_OOM;
            goto Bail;
        }
    }

    Config->Marker = 0;

Bail:
    if (FAILED(status))
    {
        if (RingBufferIsInitialized(&Config->Tape))
        {
            RingBufferTeardown(&Config->Tape);
        }

        if (Config->Appendants)
        {
            while (Config->AppendantsSize--)
            {
                BlobTeardown(Config->Appendants[Config->AppendantsSize]);
            }
            free(Config->Appendants);
        }
    }

    return status;
}

int
CyclicTagWriteTape(
    CyclicTag *Config,
    Blob *Contents
    )
{
    int status = 0;

    // initial tape
    status = RingBufferPush(&Config->Tape, Contents->Data, Contents->Size);
    if (FAILED(status))
    {
        // tape full
        goto Bail;
    }

Bail:
    return status;
}

int
CyclicTagStep(
    CyclicTag *Config
    )
{
    int status = 0;
    uint8_t word;

    status = RingBufferPop(&Config->Tape, &word, 1);
    if (FAILED(status))
    {
        // we should halt (ran out of tape)
        goto Bail;
    }

    if (word)
    {
        Blob *appendant = Config->Appendants[Config->Marker];

        status = RingBufferPush(&Config->Tape, appendant->Data, appendant->Size);
        if (FAILED(status))
        {
            // runtime error (tape too small)
            goto Bail;
        }
    }
    Config->Marker = (Config->Marker + 1) % Config->AppendantsSize;

Bail:
    return status;
}

void
CyclicTagDump(
    CyclicTag *Config
    )
{
    uint64_t i;

    printf("marker:\t#%ju -> ", Config->Marker);
    Blob *appendant = Config->Appendants[Config->Marker];
    for (i = 0; i < appendant->Size; ++i)
    {
        printf("%d ", !!appendant->Data[i]);
    }
    printf("\n");

    printf("Queue:\t");
    if (Config->Tape.ActiveSize > 0)
    {
        uint8_t *data = malloc(Config->Tape.ActiveSize);
        if (data == NULL)
        {
            TagWarn("malloc");
            return;
        }

        if (FAILED(RingBufferPeek(&Config->Tape, data, Config->Tape.ActiveSize)))
        {
            TagWarnx("RingBufferPeek");
            free(data);
            return;
        }

        printf("[%d] ", !!data[0]);
        for (i = 1; i < Config->Tape.ActiveSize; ++i)
        {
            printf("%d ", !!data[i]);
        }
        printf("\n");
        free(data);
    }
    printf("\n");
}

/*
static
int
TestCyclicTag(
    void
    )
{
    int status = 0;
    CyclicTag system;
    uint8_t appendant0[] = { 1, 1 };
    uint8_t appendant1[] = { 1, 0 };
    Blob appendants[2];
    Blob *whatever[ARRAY_SIZE(appendants)];

    status = BlobInitialize(&appendants[0], appendant0, sizeof(appendant0));
    if (FAILED(status))
    {
        TagWarnx("BlobInitialize: 0");
        goto Bail;
    }
    whatever[0] = &appendants[0];

    status = BlobInitialize(&appendants[1], appendant1, sizeof(appendant1));
    if (FAILED(status))
    {
        TagWarnx("BlobInitialize: 1");
        goto Bail;
    }
    whatever[1] = &appendants[1];

    status = CyclicTagInitialize(&system, &whatever[0], ARRAY_SIZE(appendants), 0x100);
    if (FAILED(status))
    {
        TagWarnx("CyclicTagInitialize");
        goto Bail;
    }

    uint8_t tapeData[] = {1};
    Blob contents = {
        .Data = tapeData,
        .Size = sizeof(tapeData),
        .MaxSize = sizeof(tapeData)
    };
    status = CyclicTagWriteTape(&system, &contents);
    if (FAILED(status))
    {
        TagWarnx("CyclicTagWriteTape");
        goto Bail;
    }

    uint64_t steps;
    for (steps = 0; ; ++steps)
    {
        CyclicTagDump(&system);

        status = CyclicTagStep(&system);
        if (FAILED(status))
        {
            TagWarnx("CyclicTagStep");
            printf("Appendant: %ld\n", system.Marker);
            RingBufferDump(&system.Tape);
            break;
        }
    }

    printf("steps: %lx\n", steps);

Bail:
    return FAILED(status);
}

int
main(void)
{
    return TestCyclicTag();
}
*/
