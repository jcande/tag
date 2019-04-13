#pragma once

#define MAX_LABEL   1024
#define WORD_WIDTH  24

typedef enum _SimpleOp
{
    Left,
    Right,

    Set,
    Unset,

    Get,
    Put,
} SimpleOp;

typedef struct _WmachineState
{
    Module     *Module;

    //
    // This is the address of the basic block we are processing.
    //
    uint32_t    BBAddr;

    //
    // This is just a counter that is incremented whenever we "Gen" something.
    //
    uint32_t    Counter;

    //
    // This is a pointer to the current instruction that we are processing.
    //
    Inst       *Current;

    //
    // This is where on the tape the Head is pointing to.
    //
    int64_t     Head;
} WmachineState;

//
// The registers start at offset 0 of the tape.
//
#define REG_ADDR(R) ((R) * WORD_WIDTH)

#define BB_PREFIX   "_BB_"

typedef int STATUS;
#define CHECK(Cond)                         \
if ((Cond) != SUCCESS) {                    \
    error("Shiver in eternal darkness");    \
}
#define SUCCESS  (0)
#define FAIL     (-1)
