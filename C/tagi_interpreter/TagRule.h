#pragma once

#include <stdbool.h>

#include "tommyhashlin.h"

#include "Blob.h"
#include "Util.h"

//
// XXX The problem of IO. We can either put all of the information directly in
// the rule it applies to (e.g., nothing for pure rules, output bit for output
// rules, and input appendents for input rules) or we can have distinct maps
// that contain the IO information. The former takes up more space but it
// straightforward, the latter is more space efficient but complex. For the
// moment I am going to put everything in the TagRule struct. If this does not
// scale then I can refactor it out. It is hard for me to not optimize this
// problem away right now but I think this is a good experience.
//

typedef struct _PureRule
{
    Blob Appendant;
} PureRule;

typedef struct _InputRule
{
    //
    // The idea here is that when we pop an "input" symbol we are essentially
    // performing a computation similar to the w-machine instructions we
    // emulate. This means that the input bit determines, after the fact
    // strangely enough, WHICH rule we "actually" popped. In this sense, the
    // input symbol is a stand-in for the true symbol which is only known after
    // we interact with the outside world. At this point, once the ambiguity is
    // resolved, we can go ahead and follow the wishes of this true symbol and
    // that involves adding its chosen symbols to the end of the queue.
    //
    Blob Appendant0;
    Blob Appendant1;
} InputRule;

typedef struct _OutputRule
{
    Blob Appendant;
    //
    // The output rule is very simple because everything is known; all we are
    // doing is transmitting an idea. Whenever we encounter an output rule, all
    // we need to know is what it means and that is determined entirely by what
    // bit it is representing. This naturally lets us pass this information
    // onto the next layer that will send it to the outside world. Those
    // details do not concern us.
    //
    bool Bit;
} OutputRule;

typedef enum _IoSelector
{
    IoSel_Output = 0,
    IoSel_Input = 1,
    IoSel_Pure = 2,

    IoSel_Max,
    IoSel_Min = IoSel_Output,
} IoSelector;

extern char *IoSelectorNames[IoSel_Max];

typedef struct _TagRule
{
    Blob Symbol;

    IoSelector Style;
    union {
        PureRule Pure;

        InputRule In;

        OutputRule Out;
    };

    // XXX HOW DO
    //DebugData X;

    tommy_node Node;
} TagRule;


void
TagRuleTeardown(
    TagRule *Obj
    );

void
TagRuleTeardownComplete(
    TagRule *Obj
    );

STATUS
TagRuleDupe(
    TagRule *Source,
    TagRule **Destination
    );

int
TagRuleCompare(
    const void *Arg,    /* Blob* */
    const void *Obj     /* TagRule* */
    );

void
TagRuleDump(
    TagRule *Rule
    );
