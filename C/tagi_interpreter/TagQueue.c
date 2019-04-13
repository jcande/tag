#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <assert.h>

#include <err.h>

#include <stdio.h>

#include "TagQueue.h"
#include "Blob.h"
#include "RingBuffer.h"
#include "Util.h"

typedef struct _TokenSymbol
{
    Blob Value;
    uint64_t Count;
} TokenSymbol;


static
STATUS
TagQueuePushSymbol(
    TagQueue *Q,
    void *AppendantSymbol,
    uint64_t SymbolCount
    );

static
STATUS
TagQueuePushAppendant(
    TagQueue *Q,
    Blob *Appendant
    );

STATUS
TagQueueInitialize(
        TagQueue *Q,
        uint32_t DeletionNumber,
        uint32_t SymbolSize
    )
{
    STATUS status = ENV_OK;

    assert(Q != NULL);
    assert(DeletionNumber >= 2);
    assert(SymbolSize > 0);

    memset(Q, 0, sizeof(*Q));

    Q->DeletionNumber = DeletionNumber;
    Q->SymbolSize = SymbolSize;

    Q->Cache.Symbol.Data = NULL;
    Q->Cache.Symbol.Size = 0;
    Q->Cache.Symbol.MaxSize = SymbolSize;

    status = RingBufferInitialize(
                &Q->Queue,
                sizeof(TokenSymbol*),
                RingBuffer_Expandable);
    if (FAILED(status))
    {
        goto Bail;
    }

    Q->Cache.Dirty = false;

Bail:
    if (FAILED(status))
    {
        TagQueueTeardown(Q);
    }

    return status;
}

void
TagQueueTeardown(
    TagQueue *Q
    )
{
    if (RingBufferIsInitialized(&Q->Queue))
    {
        TokenSymbol *symbol;

        // XXX this is sloppy and error-prone. Is there a better way (including
        // modifying RingBuffer)?
        while (SUCCEEDED(RingBufferPop(&Q->Queue,
                        &symbol,
                        sizeof(TokenSymbol*))))
        {
            free(symbol);
        }

        RingBufferTeardown(&Q->Queue);
    }
}

static
STATUS
TagQueueFlush(
    TagQueue *Q
    )
{
    STATUS status = ENV_OK;

    TokenSymbol *symbol = malloc(sizeof(*symbol));
    if (symbol == NULL)
    {
        TagWarnx("malloc: TokenSymbol");
        BAIL(status = ENV_OOM);
    }

    //
    // Copy the blob. The pointers are all to owned memory and we
    // aren't modifying them so it's ok.
    //
    symbol->Value = Q->Cache.Symbol;
    symbol->Count = Q->Cache.SymbolCount / Q->DeletionNumber;
    status = RingBufferPush(
                &Q->Queue,
                &symbol,
                sizeof(TokenSymbol*));
    if (FAILED(status))
    {
        free(symbol);
        goto Bail;
    }

    if ((Q->Cache.SymbolCount % Q->DeletionNumber) != 0)
    {
        //
        // We still have some residue. We flushed what we could but our cache
        // remains valid.
        //
        Q->Cache.SymbolCount %= Q->DeletionNumber;
    }
    else
    {
        //
        // The cache is now empty.
        //
        Q->Cache.Symbol.Data = NULL;
        Q->Cache.SymbolCount = 0;
        Q->Cache.Dirty = false;
    }

Bail:
    return status;
}

static
STATUS
TagQueuePushSymbol(
    TagQueue *Q,
    void *AppendantSymbol,
    uint64_t SymbolCount
    )
{
    STATUS status = ENV_OK;
    bool initializeCache = false;

    assert((Q->Cache.SymbolCount % Q->DeletionNumber) == 0);
    assert(SymbolCount >= 1);
    assert(SymbolCount <= Q->DeletionNumber);

    if (Q->Cache.Dirty)
    {
        uint64_t newCount;
        if (memcmp(Q->Cache.Symbol.Data,
                    AppendantSymbol,
                    Q->SymbolSize) == 0 &&
			!__builtin_add_overflow(Q->Cache.SymbolCount,
				SymbolCount,
				&newCount))
        {
			//
			// The pushed symbol is the same as the cache so all we need to
			// do is increment the number of symbols we have behind it.
			//

			Q->Cache.SymbolCount = newCount;
		}
		else
		{
			//
			// Flush the cache as it is either too big or stale. This function
			// is only called on a filled boundary so we must reinitialize the
			// cache after a flush.
			//

			CHECK(status = TagQueueFlush(Q));

			initializeCache = true;
		}
    }
    else
    {
        //
        // The cache is pristine. Time to put it to use.
        //

        initializeCache = true;
    }

    if (initializeCache)
    {
        //
        // Fresh cache!
        //

        Q->Cache.Dirty = true;
        Q->Cache.Symbol.Data = AppendantSymbol;
        Q->Cache.Symbol.Size = Q->SymbolSize;
        Q->Cache.SymbolCount = SymbolCount;
    }

Bail:
    return status;
}

static
inline
STATUS
TagQueuePushAppendant(
    TagQueue *Q,
    Blob *Appendant
    )
{
    STATUS status = ENV_OK;
    uint64_t i, residue, appendantCount, fill;

    assert((Appendant->Size % Q->SymbolSize) == 0);

    for (;;)
    {
        //
        // How much of the current "bucket" sticks out?
        //
        residue = Q->Cache.SymbolCount % Q->DeletionNumber;

        appendantCount = Appendant->Size / Q->SymbolSize;
        //
        // How many symbols do we need to fill the bucket?
        //
        fill = MIN(appendantCount, (Q->DeletionNumber - residue) % Q->DeletionNumber);

        uint64_t newCount;
        if (__builtin_add_overflow(Q->Cache.SymbolCount, fill, &newCount))
        {
            //
            // We have to flush the cache! Then retry with updated values.
            //

            CHECK(status = TagQueueFlush(Q));

            continue;
        }

        //
        // N.B., The only time an overflow can ever occur is if fill is
        // nonzero. In this case, the cache cannot be completely flushed as it
        // is not completely full. Due to this, we never have to worry about
        // re-initializing the cache and can simply fill up to a boundary in
        // this step and then add the symbol as normal in the main loop.
        //
        // Similarly, the loop should only ever be executed twice.
        //
        Q->Cache.SymbolCount = newCount;

        break;
    }

    for (i = fill; i < appendantCount; i += Q->DeletionNumber)
    {
        //
        // What is the size of the current "bucket"?
        //
        uint64_t size = MIN(appendantCount - i, Q->DeletionNumber);

        //
        // Translate our symbol offset from logical into byte wise.
        //
        uint64_t offset;
        if (__builtin_mul_overflow(i, Q->SymbolSize, &offset))
        {
            BAIL(status = ENV_INT_OVERFLOW);
        }

        CHECK(status = TagQueuePushSymbol(Q, &Appendant->Data[offset], size));
    }

Bail:
    return status;
}

STATUS
TagQueuePush(
    TagQueue *Q,
    Blob *Appendant,
    uint64_t Repetitions
    )
{
    STATUS status = ENV_OK;

    assert(Q != NULL);
    assert(Appendant != NULL || Repetitions == 0);
    assert(Repetitions > 0 && Appendant->Data != NULL);
    assert(Repetitions > 0 && (Appendant->Size % Q->SymbolSize) == 0);

    // XXX this might be a good optimization opportunity
    while (Repetitions--)
    {
        CHECK(status = TagQueuePushAppendant(Q, Appendant));
    }

Bail:
    return status;
}

// XXX The caller is NOT allowed to modify Blob (RO). The data also dies when
// the containing TagSystem dies.
STATUS
TagQueuePop(
    TagQueue *Q,
    Blob *Symbol,
    uint64_t *Repetitions
    )
{
    STATUS status = ENV_OK;

    assert(Q != NULL);
    assert(Symbol != NULL);
    assert(Repetitions != NULL);

    if (!RingBufferIsEmpty(&Q->Queue))
    {
        //
        // The queue has data! Just use it as normal.
        //

        TokenSymbol *symbol;

        CHECK(status = RingBufferPop(&Q->Queue,
                &symbol,
                sizeof(TokenSymbol*)));

        *Symbol = symbol->Value;
        *Repetitions = symbol->Count;

        free(symbol);
    }
    else if (Q->Cache.SymbolCount >= Q->DeletionNumber)
    {
        //
        // Even though there is nothing in the queue we still have enough
        // cached symbols to pop at least 1.
        //

        *Symbol = Q->Cache.Symbol;
        *Repetitions = Q->Cache.SymbolCount / Q->DeletionNumber;

        Q->Cache.SymbolCount %= Q->DeletionNumber;
        Q->Cache.Dirty = (Q->Cache.SymbolCount != 0);
        if (!Q->Cache.Dirty)
        {
            Q->Cache.Symbol.Data = NULL;
        }
    }
    else
    {
        //
        // There is not enough data for us to return anything yet.
        //

        TagWarnx("Nothing to pop");
        BAIL(status = TAGQ_QUEUE_TOO_SMALL);
    }

Bail:
    return status;
}

void
TagQueueDump(
    TagQueue *Q
    )
{
    (void) Q;
}
