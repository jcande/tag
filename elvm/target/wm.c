#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <target/util.h>
#include <ir/ir.h>

#define DEBUG
#define LOG printf
#define UNREF_PARAM(P)  ((void) (P))
#define ARRAY_SIZE(Arr) (sizeof(Arr) / sizeof((Arr)[0]))
#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))
#define MAX(A, B)   (((A) > (B)) ? (A) : (B))

//
// wm.h
//

#define BYTE_WIDTH  8
#define MAX_LABEL   1024
#define WORD_WIDTH  16
#define GOTO_TRANSITION_HEAD    (REG_ADDR(PC) + WORD_WIDTH - 1)
#define BB_HEAD                 (REG_ADDR(PC))
#define MEM_TRANSITION_HEAD     (REG_ADDR(PC) + WORD_WIDTH - 1)

#define INIT_LABEL(STATE, NAME) CHECK(WmachGenLabel((STATE),    \
    #NAME,                                                      \
    &NAME[0], sizeof(NAME)))

typedef int64_t reg_t;

// XXX This might match the enum (Reg) in ir.h
typedef enum _Registers
{
    RA = 0,
    RB = 1,
    RC = 2,
    RD = 3,
//    SP = 4,
//    BP = 5,
    PC = 6,
    NextPC = 7,

    Flags = 8,

    //
    // N.B., You need to be EXTREMELY careful about re-using these scratch
    // registers. If you EVER call into an operation you must make sure it
    // doesn't clobber your registers. Don't be like me and just make a new
    // register for you to use.
    //

    Scratch0 = 9,
    PutScratch  = Scratch0,
    AddSource   = Scratch0,
    SubSource   = Scratch0,
    CmpSource   = Scratch0,
    JmpSource   = Scratch0,
    // value (0->1 store, 0<-1 load)
    MemAddress  = Scratch0, // store MemSource into memory

    AddTmp   = 10,
    SubTmp   = 11,
    CmpTmp   = 12,
    MemValue = 13,
    MemReg   = 14,

    ConstantOne = 15,

    // XXX Not a register... might want a better enum name
    MemoryBase = 16,
} Registers;

typedef enum _FlagValues
{
    ZeroFlag = 0,
    CarryFlag = 1,

    //
    // This flag combines the previous flags. Since we know beforehand which
    // condition we care about, we will set this flag. This simplifies
    // branching as it can just seek to this.
    //
    TrueFlag = 2,

    //
    // If LoadStoreFlag is unset, then a load is requested. If it is set, then
    // a store is requested.
    //
    LoadStoreFlag = 3,

    //
    // This flag signifies that a conditional jump is being executed.
    // N.B., This is ONLY consumed by goto dispatching. If any other consumers
    // come into existence then they need to tread lightly.
    //
    CondFlag = 4,

    // XXX max must be less than WORD_WIDTH
} FlagValues;

typedef enum _SimpleOp
{
    Left,
    Right,

    Set,
    Unset,

    Get,
    Put,

    Debug,
} SimpleOp;

typedef int64_t Tape;

//
// Memory structures
//
typedef enum _StatusFlags {
    // XXX Maybe we should pay the initialization cost and have initialBlock and initialRegion

    StatusStartingBlock = 0,
    // 0: Load, 1: Store
    StatusLoadStore = 1,
    StatusCarry = 2,
    StatusConstantOne = 3,
    StatusZero = 4,

    // XXX max must be less than WORD_WIDTH
} StatusFlags;

typedef struct _Word {
    uint8_t Data[WORD_WIDTH / BYTE_WIDTH];
} Word;

typedef struct _Control {
    Word Status;
    Word Data;
    Word Address;
    Word Temp; // Copy the working byte here to test for zero
} Control;

typedef struct _Block {
    Control Ctl;
    Word Values[256];
} Block;

//
// This is the structure we are working with with the base at MemoryBase.
//
typedef struct _Memory {
    Block Blocks[256];
} Memory;

#define SWAP(B) ((((B) * 0x0802LU & 0x22110LU) | ((B) * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16)
#define H2N(S)  ((SWAP(((S) >> 8) & 0xff) << 0) | (SWAP(((S) >> 0) & 0xff) << 8))

#define OFFSETOF(T, F)  (offsetof(T, F) * BYTE_WIDTH)
#define OFFSET_BYTE(B)  ((B) * BYTE_WIDTH)

#define MEMORY_VALUES       ( OFFSETOF(Memory, Blocks)          \
                            + OFFSETOF(Block, Values))
// Memory.Regions[0].Blocks[0].Ctl
#define MEMORY_CONTROL      ( OFFSETOF(Memory, Blocks)          \
                            + OFFSETOF(Block, Ctl))

// Memory.Regions[0].Blocks[0].Ctl.Status + Flag
#define MEMORY_STATUS(_Flag)    ( MEMORY_CONTROL                \
                                + OFFSETOF(Control, Status)     \
                                + (_Flag))
#define MEMORY_DATA     (MEMORY_CONTROL + OFFSETOF(Control, Data))
#define MEMORY_ADDR     (MEMORY_CONTROL + OFFSETOF(Control, Address))
#define MEMORY_TEMP     (MEMORY_CONTROL + OFFSETOF(Control, Temp))


typedef struct _WmachineState
{
    Module     *Module;

    //
    // This is the address of the last basic block in the program.
    //
    uint32_t    MaxBb;

    //
    // This is just a counter that is incremented whenever we "Gen" something.
    // The intent is to be combined with other identifiers to create globally
    // unique names.
    //
    uint32_t    Counter;

    //
    // This is a pointer to the current instruction that we are processing.
    //
    Inst       *Current;

    //
    // This is where on the tape the Head is pointing to.
    //
    Tape        Head;
} WmachineState;

typedef bool(*BinOp)(bool, bool);

//
// The registers start at offset 0 of the tape.
//
#define REG_ADDR(R) ((R) * WORD_WIDTH)

#define BB_PREFIX               "_BB_"
#define FIN_LABEL               "__thank_you_for_your_attention__bye"
#define GOTO_DISPATCH_LABEL     "__GotoDispatch"
#define MEM_DISPATCH_LABEL      "__MemDispatch"

typedef int STATUS;
#define CHECK(Cond)                         \
if ((Cond) != SUCCESS) {                    \
    error("Shiver in eternal darkness");    \
}
#define SUCCESS  (0)
#define FAIL     (-1)

//
// /wm.h
//

static
inline
STATUS
WmachGenLabel(
    WmachineState *State,
    const char *Note,
    char *Buffer,
    size_t Size
    );

static
void
WmachGotoTransition(
    WmachineState *State
    );

static
void
DbgPrintRange(
    WmachineState *State,
    int64_t Base,
    uint64_t Size
    );

static
void
MemInitialize(
    WmachineState *State
    );

static
void
WmachProduceMemTable(
    WmachineState *State
    );


//
// Tape operations
//

static
Tape
TapePreserve(
    WmachineState *State
    );

static
void
TapeRestore(
    WmachineState *State,
    Tape Head
    );

static
int64_t
TapeSeek(
    WmachineState *State,
    Tape New
    );

static
void
TapeCopyBit(
    WmachineState *State,
    int64_t Dest,
    int64_t Source
    );

static
inline
void
TapeCopyWord(
    WmachineState *State,
    int64_t Dest,
    int64_t Source
    );

static
void
TapeWriteBit(
    WmachineState *State,
    int64_t Dest,
    bool Bit
    );

static
void
TapeWriteMemory(
    WmachineState *State,
    int64_t Dest,
    int Imm,
    uint32_t Length
    );

static
void
TapeWriteWord(
    WmachineState *State,
    int64_t Dest,
    int Imm
    );




static
void
EmitBranch(
    const char *TrueLabel,
    const char *FalseLabel
    );

static
void
EmitLabel(
    const char *Name
    );

static
void
EmitSimple(
    SimpleOp Code
    );

static
inline
uint32_t
WmachGen(
    WmachineState *State
    );

static
void
EmitHalt(
    void
    );



//
// Logging functions
//

static
void
VEmitFmt(
    const char *Fmt,
    va_list Args
    )
{
    (void) vprintf(Fmt, Args);
}

static
void
EmitFmt(
    const char *Fmt,
    ...
    )
{
    va_list args;

    va_start(args, Fmt);
    VEmitFmt(Fmt, args);
    va_end(args);
}


//
// ALU functions
//

static
bool
BinOpAbjunction(
    bool A,
    bool B
    )
{
    bool result = A;
    if (A)
    {
        result = !B;
    }
    else
    {
        result = 0;
    }
    return result;
}


static
bool
BinOpAnd(
    bool A,
    bool B
    )
{
    return (A && B);
}

static
bool
BinOpOr(
    bool A,
    bool B
    )
{
    return (A || B);
}

static
bool
BinOpXor(
    bool A,
    bool B
    )
{
    return (A ^ B);
}

static
void
AluBinOpBit(
    WmachineState *State,
    BinOp Operation,
    reg_t Dest,
    reg_t OpA,
    reg_t OpB
    )
{
    char end_label[MAX_LABEL];
    char srcA_set[MAX_LABEL];
    char srcA_unset[MAX_LABEL];
    Tape initialHead, nestedHead;
    int i, j;

    CHECK(WmachGenLabel(State, "end", &end_label[0], sizeof(end_label)));
    CHECK(WmachGenLabel(State, "set_srcA", &srcA_set[0], sizeof(srcA_set)));
    CHECK(WmachGenLabel(State, "unset_srcA", &srcA_unset[0], sizeof(srcA_unset)));

    initialHead = TapeSeek(State, OpA);
    EmitFmt("\n");
    EmitBranch(&srcA_set[0], &srcA_unset[0]);
    for (i = 0; i < 2; ++i)
    {
        char srcB_set[MAX_LABEL];
        char srcB_unset[MAX_LABEL];

        TapeRestore(State, initialHead);

        CHECK(WmachGenLabel(State,
            "set_srcB",
            &srcB_set[0],
            sizeof(srcB_set)));
        CHECK(WmachGenLabel(State,
            "unset_srcB",
            &srcB_unset[0],
            sizeof(srcB_unset)));

        if (i == 0)
        {
            EmitLabel(&srcA_unset[0]);
        }
        else
        {
            EmitLabel(&srcA_set[0]);
        }

        nestedHead = TapeSeek(State, OpB);
        EmitFmt("\n");
        EmitBranch(&srcB_set[0], &srcB_unset[0]);
        for (j = 0; j < 2; ++j)
        {
            TapeRestore(State, nestedHead);

            if (j == 0)
            {
                EmitLabel(&srcB_unset[0]);
            }
            else
            {
                EmitLabel(&srcB_set[0]);
            }

            bool newBit = Operation(i != 0, j != 0);
            TapeWriteBit(State, Dest, newBit);
            EmitFmt("\n");
            EmitBranch(&end_label[0], &end_label[0]);
        }
    }

    EmitLabel(&end_label[0]);
}

static
void
AluAddBit(
    WmachineState *State,
    reg_t Dest,
    reg_t OpA,
    reg_t OpB,
    reg_t Carry
    )
{
    enum TmpAddrs {
        Result = 0,
        MidCarry = 1,
    };
    reg_t tmpAddr = REG_ADDR(AddTmp);

    // XXX everyone that uses a scratch register should have this
    // XXX Should actually check a range
    assert(Dest != tmpAddr);
    assert(OpA != tmpAddr);
    assert(OpB != tmpAddr);

    //
    // AddTmp[Result] = (OpA ^ OpB) ^ carryAddr
    // AddTmp[MidCarry] = (OpA ^ OpB) & carryAddr
    // We must use intermediate storage as we still need the original OpA,
    // and OpB bits.
    //
    AluBinOpBit(State, BinOpXor, tmpAddr + Result, OpA, OpB);
    AluBinOpBit(State, BinOpAnd, tmpAddr + MidCarry, Carry, tmpAddr + Result);
    AluBinOpBit(State, BinOpXor, tmpAddr + Result, Carry, tmpAddr + Result);

    //
    // carryOut = AddTmp[MidCarry] || (OpA & OpB)
    // We now calculate the new carry.
    //
    AluBinOpBit(State, BinOpAnd, Carry, OpA, OpB);
    AluBinOpBit(State, BinOpOr, Carry, Carry, tmpAddr + MidCarry);

    //
    // Finally, copy over the result to the proper destination.
    //
    TapeCopyBit(State, Dest, tmpAddr + Result);
}

static
void
AluAddWord(
    WmachineState *State,
    reg_t Dest,
    reg_t OpA,
    reg_t OpB
    )
{
    reg_t carryAddr = REG_ADDR(Flags) + CarryFlag;
    reg_t offset;

    //
    // Clear the carry bit.
    //
    LOG("/* clear the carry bit */\n");
    TapeSeek(State, carryAddr);
    EmitSimple(Unset);
    EmitFmt("\n");

    for (offset = 0; offset < WORD_WIDTH; ++offset)
    {
        LOG("/* adding bit %ld */\n", offset);
        AluAddBit(State, Dest + offset, OpA + offset, OpB + offset, carryAddr);
    }
    EmitFmt("\n");
}

static
void
WmachProduceAdd(
    WmachineState *State
    )
{
    Value src = State->Current->src;
    Value dst = State->Current->dst;
    int64_t srcAddr;

    if (src.type == REG)
    {
        EmitFmt("/* ADD R%u, R%u */\n", dst.reg, src.reg);
        srcAddr = REG_ADDR(src.reg);
    }
    else if (src.type == IMM)
    {
        EmitFmt("/* ADD R%u, 0x%x */\n", dst.reg, src.imm);
        srcAddr = REG_ADDR(AddSource);
        TapeWriteWord(State, srcAddr, src.imm);
        EmitFmt("\n");
    }
    else
    {
        error("add: src: Unrecognized Value::type: %x", src.type);
    }

    if (dst.type != REG)
    {
        error("add: dst: Unrecognized Value::type: %x", dst.type);
    }

    AluAddWord(State, REG_ADDR(dst.reg), REG_ADDR(dst.reg), srcAddr);
}

static
void
AluInvertBit(
    WmachineState *State,
    int64_t Dest,
    int64_t Source
    )
{
    char true_label[MAX_LABEL];
    char false_label[MAX_LABEL];
    char end_label[MAX_LABEL];
    Tape initialHead;

    CHECK(WmachGenLabel(State, "true", &true_label[0], sizeof(true_label)));
    CHECK(WmachGenLabel(State, "false", &false_label[0], sizeof(false_label)));
    CHECK(WmachGenLabel(State, "end", &end_label[0], sizeof(end_label)));

    initialHead = TapeSeek(State, Source);
    EmitFmt("\n");

    EmitBranch(&true_label[0], &false_label[0]);

    //
    // Write a 0.
    //
    EmitLabel(&true_label[0]);
    TapeRestore(State, initialHead);
    TapeSeek(State, Dest);
    EmitSimple(Unset);
    EmitFmt("\n");
    EmitBranch(&end_label[0], &end_label[0]);

    //
    // Write a 1.
    //
    EmitLabel(&false_label[0]);
    TapeRestore(State, initialHead);
    TapeSeek(State, Dest);
    EmitSimple(Set);
    EmitFmt("\n");

    EmitLabel(&end_label[0]);
}

static
void
AluInvertWord(
    WmachineState *State,
    int64_t Dest,
    int64_t Source
    )
{
    int64_t offset;

    for (offset = 0; offset < WORD_WIDTH; ++offset)
    {
        AluInvertBit(State, Dest + offset, Source + offset);
    }
    EmitFmt("\n");
}

static
void
AluSubWord(
    WmachineState *State,
    reg_t Dest,
    reg_t OpA,
    reg_t OpB
    )
{
    reg_t tmpAddr = REG_ADDR(SubTmp);

    // XXX Should actually check a range
    assert(Dest != tmpAddr);
    assert(OpA != tmpAddr);
    assert(OpB != tmpAddr);

    //
    // Two's compliment (SubTmp := ~OpB + 1).
    //
    AluInvertWord(State, tmpAddr, OpB);
    AluAddWord(State, tmpAddr, tmpAddr, REG_ADDR(ConstantOne));

    //
    // Subtract (Dest := OpA + (~OpB + 1)).
    //
    AluAddWord(State, Dest, OpA, tmpAddr);

    //
    // We are subtracting, so flip the carry flag to reflect it.
    //
    AluInvertBit(State, REG_ADDR(Flags) + CarryFlag, REG_ADDR(Flags) + CarryFlag);
}

static
void
WmachProduceSub(
    WmachineState *State
    )
{
    Value src = State->Current->src;
    Value dst = State->Current->dst;
    int64_t srcAddr;

    srcAddr = REG_ADDR(SubSource);
    if (src.type == REG)
    {
        EmitFmt("/* SUB R%u, R%u */\n", dst.reg, src.reg);
        TapeCopyWord(State, srcAddr, REG_ADDR(src.reg));
    }
    else if (src.type == IMM)
    {
        EmitFmt("/* SUB R%u, 0x%x */\n", dst.reg, src.imm);
        TapeWriteWord(State, srcAddr, src.imm);
        EmitFmt("\n");
    }
    else
    {
        error("sub: src: Unrecognized Value::type: %x", src.type);
    }

    if (dst.type != REG)
    {
        error("sub: dst: Unrecognized Value::type: %x", dst.type);
    }

    AluSubWord(State, REG_ADDR(dst.reg), REG_ADDR(dst.reg), srcAddr);
}

static
void
AluCalculateZf(
    WmachineState *State,
    reg_t Source
    )
{
    int64_t offset;
    reg_t zeroAddr;

    zeroAddr = REG_ADDR(Flags) + ZeroFlag;

    //
    // Set the Zero flag
    //
    TapeWriteMemory(State, zeroAddr, 1, 1);

    for (offset = 0; offset < WORD_WIDTH; ++offset)
    {
        AluBinOpBit(State,
            BinOpAbjunction,
            zeroAddr,
            zeroAddr,
            Source + offset);
    }
}


// Sets the True flag to OpA `CmdOp` OpB. Similarly clobbers Dest.
static
void
AluCmpWord(
    WmachineState *State,
    Op CmpOp,
    reg_t Dest,
    reg_t OpA,
    reg_t OpB
    )
{
    reg_t zeroAddr  = REG_ADDR(Flags) + ZeroFlag;
    reg_t carryAddr = REG_ADDR(Flags) + CarryFlag;
    reg_t trueAddr  = REG_ADDR(Flags) + TrueFlag;

    // XXX This actually now gets complicated as we are using scratch
    // registers IN the "general" function. SubOne is an alias of Scratch1. If
    // any caller is using Scratch1 for something then this will explode.

    assert(CmpOp >= EQ && CmpOp <= GE);

    TapeWriteBit(State, trueAddr, 0);
    EmitFmt("\n");

    //
    // Calculate CF.
    //
    AluSubWord(State, Dest, OpA, OpB);

    //
    // Calculate ZF.
    //
    AluCalculateZf(State, Dest);

    switch (CmpOp) {
    case EQ:
        //
        // TF := ZF
        //
        TapeCopyBit(State, trueAddr, zeroAddr);
        break;

    case NE:
        //
        // TF := ~ZF
        //
        AluInvertBit(State, trueAddr, zeroAddr);
        break;

    case LE:
    case GT:
        //
        // TF := CF | ZF
        //
        AluBinOpBit(State, BinOpOr, trueAddr, zeroAddr, carryAddr);

        if (CmpOp == GT)
        {
            //
            // TF := ~(CF | ZF)
            //
            AluInvertBit(State, trueAddr, trueAddr);
        }
        break;

    case GE:
        //
        // TF := ~CF
        //
        AluInvertBit(State, trueAddr, carryAddr);
        break;

    case LT:
        //
        // TF := CF
        //
        TapeCopyBit(State, trueAddr, carryAddr);
        break;

    default:
        error("cmp word: Unrecognized op: %x", CmpOp);
    }

    TapeCopyBit(State, Dest, trueAddr);
    TapeWriteMemory(State, Dest + 1, 0, WORD_WIDTH - 1);
    EmitFmt("\n");
}

// XXX This confuses the interface. Jcc should NOT call this but should invoke
// the ALU sub-function directly. All logic that is not just putting registers
// in the proper scratch space is happening in the wrong function.
static
void
WmachProduceCompare(
    WmachineState *State
    )
{
#define JCC_TO_CMP(Jcc) ((Jcc) + 8)
    const char *opNames[] = {
        "EQ", "NE",
        "LT", "GT",
        "LE", "GE",
    };
    Value src = State->Current->src;
    Value dst = State->Current->dst;
    reg_t opB;
    reg_t destAddr;
    Op op;

    // XXX there is a normalize function in util.c. Use that!
    if (State->Current->op >= JEQ && State->Current->op <= JGE)
    {
        //
        // Since we're a jmp, we will discard the result.
        //
        destAddr = REG_ADDR(CmpTmp);
        op = JCC_TO_CMP(State->Current->op);
    }
    else if (State->Current->op >= EQ && State->Current->op <= GE)
    {
        //
        // The various cmp functions write the result out instead of using it
        // directly in a conditional branch operationg.
        //
        destAddr = REG_ADDR(dst.reg);
        op = State->Current->op;
    }
    else
    {
        error("cmp: Unrecognized op: %x", State->Current->op);
    }
    assert((uint64_t)(op - EQ) < ARRAY_SIZE(opNames));

    if (src.type == REG)
    {
        EmitFmt("/* CMP%s R%u, R%u */\n", opNames[op - EQ], dst.reg, src.reg);
        opB = REG_ADDR(src.reg);
    }
    else if (src.type == IMM)
    {
        EmitFmt("/* CMP%s R%u, 0x%x */\n", opNames[op - EQ], dst.reg, src.imm);
        opB = REG_ADDR(CmpSource);
        TapeWriteWord(State, opB, src.imm);
        EmitFmt("\n");
    }
    else
    {
        error("cmp: src: Unrecognized Value::type: %x", src.type);
    }

    if (dst.type != REG)
    {
        error("cmp: dst: Unrecognized Value::type: %x", dst.type);
    }

    AluCmpWord(State, op, destAddr, REG_ADDR(dst.reg), opB);
}


//
// Tape functions
//

// Should never be invoked unless by Tape*
static
void
ScanLeft(
    uint64_t Bits
    )
{
    while (Bits-- > 0)
    {
        EmitSimple(Left);
    }
}

// Should never be invoked unless by Tape*
static
void
ScanRight(
    uint64_t Bits
    )
{
    while (Bits-- > 0)
    {
        EmitSimple(Right);
    }
}

static
Tape
TapePreserve(
    WmachineState *State
    )
{
    return State->Head;
}

static
void
TapeRestore(
    WmachineState *State,
    int64_t Head
    )
{
    State->Head = Head;
}

static
int64_t
TapeSeek(
    WmachineState *State,
    int64_t New
    )
{
    int64_t head = State->Head;
    if (head > New)
    {
        ScanLeft(head - New);
    }
    else
    {
        ScanRight(New - head);
    }

    State->Head = New;

    return New;
}

static
void
TapeCopyBit(
    WmachineState *State,
    int64_t Dest,
    int64_t Source
    )
{
    Tape oldHead;
    char true_label[MAX_LABEL];
    char false_label[MAX_LABEL];
    char end_label[MAX_LABEL];

    CHECK(WmachGenLabel(State, "true", &true_label[0], sizeof(true_label)));
    CHECK(WmachGenLabel(State, "false", &false_label[0], sizeof(false_label)));
    CHECK(WmachGenLabel(State, "end", &end_label[0], sizeof(end_label)));

    //
    // Seek to the source location. Branching uses the value under the head as
    // the condition.
    //
    oldHead = TapeSeek(State, Source);
    EmitFmt("\n");
    EmitBranch(&true_label[0], &false_label[0]);

    //
    // Write a 1.
    //
    EmitLabel(&true_label[0]);
    TapeRestore(State, oldHead);
    TapeSeek(State, Dest);
    EmitSimple(Set);
    EmitFmt("\n");
    EmitBranch(&end_label[0], NULL);

    //
    // Write a 0.
    //
    EmitLabel(&false_label[0]);
    TapeRestore(State, oldHead);
    TapeSeek(State, Dest);
    EmitSimple(Unset);
    EmitFmt("\n");

    EmitLabel(&end_label[0]);
}

static
void
TapeCopyMemory(
    WmachineState *State,
    int64_t Dest,
    int64_t Source,
    uint32_t Length
    )
{
    int64_t offset;

    for (offset = 0; offset < (int64_t) Length; ++offset)
    {
        TapeCopyBit(State, Dest + offset, Source + offset);
    }
}

static
inline
void
TapeCopyWord(
    WmachineState *State,
    int64_t Dest,
    int64_t Source
    )
{
    TapeCopyMemory(State, Dest, Source, WORD_WIDTH);
    EmitFmt("\n");
}

static
void
TapeWriteBit(
    WmachineState *State,
    int64_t Dest,
    bool Bit
    )
{
    TapeSeek(State, Dest);

    if (Bit)
    {
        EmitSimple(Set);
    }
    else
    {
        EmitSimple(Unset);
    }
}

static
void
TapeWriteMemory(
    WmachineState *State,
    int64_t Dest,
    int Imm,
    uint32_t Length
    )
{
    int64_t offset;

    if (Length > WORD_WIDTH)
    {
        error("Can't write more than a word");
    }

    for (offset = 0; offset < Length; ++offset)
    {
        TapeWriteBit(State, Dest + offset, Imm & (1 << offset));
    }
}

static
void
TapeWriteWord(
    WmachineState *State,
    int64_t Dest,
    int Imm
    )
{
    TapeWriteMemory(State, Dest, Imm, WORD_WIDTH);
    EmitFmt("\n");
}

static
void
WmachProduceMov(
    WmachineState *State
    )
{
    Value src = State->Current->src;
    Value dst = State->Current->dst;

    if (src.type == REG)
    {
        LOG("/* %u: MOV R%u, R%u */\n", State->Current->pc, dst.reg, src.reg);
        TapeCopyWord(State, REG_ADDR(dst.reg), REG_ADDR(src.reg));
    }
    else if (src.type == IMM)
    {
        LOG("/* %u: MOV R%u, 0x%x */\n", State->Current->pc, dst.reg, src.imm);
        TapeWriteWord(State, REG_ADDR(dst.reg), src.imm);
    }
    else
    {
        error("mov: Unrecognized Value::type: %x", src.type);
    }

    EmitFmt("\n");
}

//
// Memory functions
//

static
void
WmachMemTransition(
    WmachineState *State
    )
{
    TapeSeek(State, MEM_TRANSITION_HEAD);
    EmitFmt("\n");
    EmitBranch(MEM_DISPATCH_LABEL, MEM_DISPATCH_LABEL);
    EmitFmt("\n\n");
}

static
void
WmachProduceMemLoad(
    WmachineState *State
    )
{
    Value src = State->Current->src;    // memory address we load from
    Value dst = State->Current->dst;    // register we load to

    if (src.type == REG)
    {
        LOG("/* %u: LD R%u, [R%u] */\n", State->Current->pc, dst.reg, src.reg);
        TapeCopyWord(State, REG_ADDR(MemAddress), REG_ADDR(src.reg));
    }
    else if (src.type == IMM)
    {
        LOG("/* %u: LD R%u, [0x%x] */\n", State->Current->pc, dst.reg, src.imm);
        TapeWriteWord(State, REG_ADDR(MemAddress), src.imm);
    }
    else
    {
        error("ld: Unrecognized Value::type: %x", src.type);
    }

    //
    // Save which register should contain the loaded value.
    //
    assert((uint32_t) dst.reg >= (uint32_t) RA && (uint32_t) dst.reg < (uint32_t) PC);
    TapeCopyWord(State, REG_ADDR(MemReg), dst.reg);

    //
    // Clearing LoadStoreFlag indicates we want to read memory.
    //
    TapeWriteBit(State, REG_ADDR(Flags) + LoadStoreFlag, 0);

    //
    // Jump to dispatch.
    //
    WmachMemTransition(State);
}

static
void
WmachProduceMemStore(
    WmachineState *State
    )
{
    //
    // These are flipped due to implementation details of EIR.
    //
    Value src = State->Current->dst;    // register value we store
    Value dst = State->Current->src;    // memory address where we store

    assert(src.type == REG);

    //
    // Copy the memory address to the proper register.
    //
    if (dst.type == REG)
    {
        LOG("/* %u: ST R%u, [R%u] */\n", State->Current->pc, src.reg, dst.reg);
        TapeCopyWord(State, REG_ADDR(MemAddress), REG_ADDR(dst.reg));
    }
    else if (dst.type == IMM)
    {
        LOG("/* %u: ST R%u, [0x%x] */\n", State->Current->pc, src.reg, dst.imm);
        TapeWriteWord(State, REG_ADDR(MemAddress), dst.imm);
    }
    else
    {
        error("st: Unrecognized Value::type: %x", dst.type);
    }

    //
    // Copy the data we wish to store to the proper register.
    //
    TapeCopyWord(State, REG_ADDR(MemValue), REG_ADDR(src.reg));

    //
    // Setting LoadStoreFlag indicates we want to write memory.
    //
    TapeWriteBit(State, REG_ADDR(Flags) + LoadStoreFlag, 1);

    //
    // Jump to dispatch.
    //
    WmachMemTransition(State);
}


//
// Branch functions (low)
//

static
void
EmitLabel(
    const char *Name
    )
{
    EmitFmt("%s:\n", Name);
}

static
void
EmitBranch(
    const char *TrueLabel,
    const char *FalseLabel
    )
{
    EmitFmt("jmp %s", TrueLabel);

    if (FalseLabel != NULL)
    {
        EmitFmt(", %s", FalseLabel);
    }

    EmitFmt("\n");
}

static
inline
STATUS
GenLabelStatic(
    const char *Note,
    uint32_t Width,
    uint64_t Value,
    char *Buffer,
    size_t Size
    )
{
    STATUS status = SUCCESS;

    //
    // This does NOT use the binary prefix and instead is an abomination.
    // Since it generates unique and predictible label names we don't mind too
    // much.
    //
    if (snprintf(Buffer,
                 Size,
                 "%s_%0*lx",
                 Note,
                 Width,
                 Value) < 0)
    {
        status = FAIL;
    }

    return status;
}

static
inline
STATUS
WmachGenLabel(
    WmachineState *State,
    const char *Note,
    char *Buffer,
    size_t Size
    )
{
    STATUS status = SUCCESS;
    uint32_t value = WmachGen(State);

    assert(State != NULL);

    if (snprintf(Buffer,
                 Size,
                 "%s_%x_%x",
                 Note,
                 State->Current->pc,
                 value) < 0)
    {
        status = FAIL;
    }

    return status;
}

static
inline
STATUS
GenLabelBb(
    int Pc,
    char *Buffer,
    size_t Size
    )
{
    STATUS status = SUCCESS;

    if (snprintf(Buffer, Size, "%s%x", BB_PREFIX, Pc) < 0)
    {
        status = FAIL;
    }

    return status;
}



//
// Goto functions (branches high)
//
// XXX We need a better branching abstraction! I spent a TON of time tracking
// down the exact same bug I've debugged a few times: diverging branch state.
// Maybe another branching function that takes/receives a context?
static
void
WmachProduceGoto(
    WmachineState *State
    )
{
    Value jmp = State->Current->jmp;
    reg_t pcAddr = REG_ADDR(PC);

    if (State->Current->op != JMP)
    {
        reg_t condAddr  = REG_ADDR(Flags) + CondFlag;

        assert(State->Current->op >= JEQ && State->Current->op <= JGE);

        //
        // Let the dispatcher know that this is a conditional jmp.
        //
        TapeWriteBit(State, condAddr, 1);
        pcAddr = REG_ADDR(NextPC);

        //
        // Perform the comparison, which stores the result in TrueFlag.
        // This logic is kinda weird as we are re-using the high-level
        // dispatch. I am not a fan of this but the code is already written and
        // the other modules seem to do it this way too.
        //
        WmachProduceCompare(State);
    }

    if (jmp.type == REG)
    {
        LOG("/* %u: JMP R%u */\n", State->Current->pc, jmp.reg);
        TapeCopyWord(State, pcAddr, REG_ADDR(jmp.reg));
    }
    else if (jmp.type == IMM)
    {
        LOG("/* %u: JMP 0x%x */\n", State->Current->pc, jmp.imm);
        TapeWriteWord(State, pcAddr, jmp.imm);
    }
    else
    {
        error("branch: Unrecognized Value::type: %x", jmp.type);
    }


    //
    // Jump to dispatch.
    //

    WmachGotoTransition(State);
}

static
void
WmachGotoTableGen(
    WmachineState *State,
    uint32_t Depth,
    uint64_t Prefix,
    uint64_t Max
    )
{
    if (Prefix <= Max && Depth == 0)
    {
        char pc_label[MAX_LABEL];

        CHECK(GenLabelBb(Prefix, &pc_label[0], sizeof(pc_label)));

        //
        // Once we've found the proper address the tape should be pointing to
        // the LSB of PC.
        //

        assert(State->Head == BB_HEAD);

        EmitBranch(&pc_label[0], &pc_label[0]);
    }
    else if ((Prefix << Depth) > Max)
    {
        //
        // This PC is bigger than any basic-block that the program contains. If
        // PC is ever this value then someone is up to no good. Bail.
        //

        EmitHalt();
    }
    else
    {
        uint64_t prefix1, prefix0;
        char prefix1_label[MAX_LABEL];
        char prefix0_label[MAX_LABEL];
        uint32_t width = WORD_WIDTH - Depth;
        int64_t savedHead;

        prefix1 = (Prefix << 1) | 1;
        prefix0 = (Prefix << 1) | 0;

        //
        // N.B., We must add 1 to the width as this is the NEXT width since it
        // is the result of having tested the current bit.
        //
        CHECK(GenLabelStatic("prefix",
            width + 1,
            prefix1,
            &prefix1_label[0],
            sizeof(prefix1_label)));
        CHECK(GenLabelStatic("prefix",
            width + 1,
            prefix0,
            &prefix0_label[0],
            sizeof(prefix0_label)));

        //
        // Seek to current bit of PC.
        //
        savedHead = TapeSeek(State, REG_ADDR(PC) + Depth - 1);
        EmitFmt("\n");

        //
        // Test the current bit of PC.
        //
        EmitBranch(&prefix1_label[0], &prefix0_label[0]);

        TapeRestore(State, savedHead);
        EmitLabel(&prefix1_label[0]);
        WmachGotoTableGen(State,
            Depth - 1,
            prefix1,
            Max);

        TapeRestore(State, savedHead);
        EmitLabel(&prefix0_label[0]);
        WmachGotoTableGen(State,
            Depth - 1,
            prefix0,
            Max);
    }
}

static
void
WmachProduceGotoTable(
    WmachineState *State
    )
{
    char fix[MAX_LABEL];
    char ready[MAX_LABEL];
    char successful[MAX_LABEL];
    char unsuccessful[MAX_LABEL];
    char join[MAX_LABEL];
    reg_t condAddr = REG_ADDR(Flags) + CondFlag;
    reg_t trueAddr = REG_ADDR(Flags) + TrueFlag;
    Tape readyHead, inHead, outHead;

    INIT_LABEL(State, fix);
    INIT_LABEL(State, ready);
    INIT_LABEL(State, successful);
    INIT_LABEL(State, unsuccessful);
    INIT_LABEL(State, join);

    //
    // N.B., This is necessary because we can be invoked at arbitrary times.
    // This is the first bit of the PC that we are to test.
    // All useres should not call us directly but should instead use
    // WmachGotoTransition which handles this interface on their behalf.
    //
    TapeRestore(State, GOTO_TRANSITION_HEAD);

    EmitLabel(GOTO_DISPATCH_LABEL);

// should be its own function {
    readyHead = TapeSeek(State, condAddr);
    EmitFmt("\n");
    EmitBranch(&fix[0], &ready[0]);
    EmitLabel(&fix[0]);

    //
    // We are dispatching a conditional. Let's check to see which PC to use.
    //

    inHead = TapeSeek(State, trueAddr);
    EmitFmt("\n");
    outHead = condAddr;
    EmitBranch(&successful[0], &unsuccessful[0]);

    //
    // The condition was true so we can use the prepared PC.
    //
    EmitLabel(&successful[0]);
    TapeRestore(State, inHead);

    TapeCopyWord(State, REG_ADDR(PC), REG_ADDR(NextPC));

    TapeSeek(State, outHead);
    EmitFmt("\n");
    EmitBranch(&join[0], &join[0]);

    //
    // The condition was false so we must calculate the next PC ourselves.
    //
    EmitLabel(&unsuccessful[0]);
    TapeRestore(State, inHead);

    AluAddWord(State, REG_ADDR(PC), REG_ADDR(PC), REG_ADDR(ConstantOne));

    TapeSeek(State, outHead);
    EmitFmt("\n");
    EmitBranch(&join[0], &join[0]);


    EmitLabel(&join[0]);

    //
    // Consume the cond flag and reset the head to where the ready branch
    // expects.
    //
    TapeWriteBit(State, condAddr, 0);
    TapeSeek(State, readyHead);
    EmitFmt("\n");
// should be its own function }

    //
    // PC is now properly calculated and ready to dispatch the next
    // instruction.
    //
    EmitLabel(&ready[0]);
    WmachGotoTableGen(State, WORD_WIDTH, 0, State->MaxBb);
    EmitFmt("\n\n");
}

static
void
WmachGotoTransition(
    WmachineState *State
    )
{
    TapeSeek(State, GOTO_TRANSITION_HEAD);
    EmitFmt("\n");
    EmitBranch(GOTO_DISPATCH_LABEL, GOTO_DISPATCH_LABEL);
    EmitFmt("\n\n");
}




//
// Io functions
//

static
void
IoPutByte(
    WmachineState *State,
    int64_t Source
    )
{
    int64_t offset;

    for (offset = 0; offset < BYTE_WIDTH; ++offset)
    {
        TapeSeek(State, Source + offset);
        EmitSimple(Put);
    }
    EmitFmt("\n");
}

static
void
WmachProducePut(
    WmachineState *State
    )
{
    Value src = State->Current->src;
    int64_t srcAddr;

    if (src.type == REG)
    {
        LOG("/* %u: PUTC R%u */\n", State->Current->pc, src.reg);
        srcAddr = REG_ADDR(src.reg);
    }
    else if (src.type == IMM)
    {
        LOG("/* %u: PUTC 0x%x */\n", State->Current->pc, src.imm);
        TapeWriteMemory(State, REG_ADDR(PutScratch), src.imm, BYTE_WIDTH);
        srcAddr = REG_ADDR(PutScratch);
    }
    else
    {
        error("put: Unrecognized Value::type: %x", src.type);
    }

    IoPutByte(State, srcAddr);
}

static
void
IoGetByte(
    WmachineState *State,
    int64_t Dest
    )
{
    int64_t offset;

    for (offset = 0; offset < BYTE_WIDTH; ++offset)
    {
        TapeSeek(State, Dest + offset);
        EmitSimple(Get);
    }
}

static
void
WmachProduceGet(
    WmachineState *State
    )
{
    Value dst = State->Current->dst;

    if (dst.type == REG)
    {
        IoGetByte(State, REG_ADDR(dst.reg));
    }
    else
    {
        error("get: Unrecognized Value::type: %x", dst.type);
    }
}

//
// Misc functions
//

static
void
EmitHalt(
    void
    )
{
    //
    // Branching to an invalid address is equivalent to halting.
    //
    EmitBranch(FIN_LABEL, FIN_LABEL);
}

static
void
WmachProduceHalt(
    WmachineState *State
    )
{
    LOG("/* %u: EXIT */\n", State->Current->pc);

    EmitHalt();
}

static
inline
uint32_t
WmachGen(
    WmachineState *State
    )
{
    // XXX if this rolls-over we should bail
    return State->Counter++;
}


static
void
EmitSimple(
    SimpleOp Code
    )
{
    char raw;
    switch (Code) {
    case Left:
        raw = '<';
        break;
    case Right:
        raw = '>';
        break;
    case Set:
        raw = '+';
        break;
    case Unset:
        raw = '-';
        break;
    case Get:
        raw = ',';
        break;
    case Put:
        raw = '.';
        break;
    case Debug:
        raw = '!';
        break;
    default:
        error("Unrecognized simple op: %x\n", Code);
    }

    emit_1(raw);
}



//
// Meta functions
//

static
void
WmachInitialize(
    WmachineState *State,
    Module *Mod
    )
{
    assert(State != NULL);
    assert(Mod != NULL);

    //
    // N.B., The registers MUST start at offset 0 of the tape. If they do not
    // then at least REG_ADDR will be wrong (but likely others).
    //
    assert(RA == 0);
    assert((uint32_t) RA == (uint32_t) A);
    assert(RB == 1);
    assert((uint32_t) RB == (uint32_t) B);
    assert(RC == 2);
    assert((uint32_t) RC == (uint32_t) C);
    assert(RD == 3);
    assert((uint32_t) RD == (uint32_t) D);

    memset(State, 0, sizeof(*State));

    State->Module = Mod;
    State->Current = Mod->text;

    emit_reset();
    emit_start();

    for (Inst *inst = State->Current;
        inst != NULL;
        inst = inst->next)
    {
        if (inst->next != NULL && inst->pc != inst->next->pc)
        {
            State->MaxBb++;
        }
    }

// XXX DBG
/*
    //
    // Set up various constants.
    //
    TapeWriteWord(State, REG_ADDR(ConstantOne), 1);

    //
    // Set up the various bits of data needed for our memory scheme.
    //
    MemInitialize(State);

    //
    // Jump to first instruction.
    //
    WmachGotoTransition(State);


    //
    // Generate various structures.
    //
    WmachProduceGotoTable(State);
    WmachProduceMemTable(State);
*/
}

static
inline
bool
WmachMoreInsts(
    WmachineState *State
    )
{
    assert(State != NULL);
    return (State->Current != NULL);
}

static
inline
void
WmachNextInst(
    WmachineState *State
    )
{
    assert(State != NULL);
    assert(State->Current != NULL);

    Inst *next = State->Current->next;

    State->Current = next;
}

static
void
WmachProduceInst(
    WmachineState *State
    )
{
    switch (State->Current->op) {
    case MOV:
        WmachProduceMov(State);
        break;

    case ADD:
        WmachProduceAdd(State);
        break;
    case SUB:
        WmachProduceSub(State);
        break;

    case LOAD:
        WmachProduceMemLoad(State);
        break;
    case STORE:
        WmachProduceMemStore(State);
        break;

    case PUTC:
        WmachProducePut(State);
        break;
    case GETC:
        WmachProduceGet(State);
        break;

    case EQ:
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
        WmachProduceCompare(State);
        break;

    case JEQ:
    case JNE:
    case JLT:
    case JGT:
    case JLE:
    case JGE:
    case JMP:
        WmachProduceGoto(State);
        break;

    case EXIT:
        WmachProduceHalt(State);
        break;

    case DUMP:
        LOG("/* DUMP (nop) */\n");
        break;

    default:
        error("Unrecognized op: %x", State->Current->op);
    }
}

static
void
MemInitialize(
    WmachineState *State
    )
{
    //
    // Mark the initial region (region 0). This is how memory accesses check to
    // see if they are back in reality.
    //
    TapeWriteBit(State,
        REG_ADDR(MemoryBase) + MEMORY_STATUS(StatusStartingBlock),
        1);
}

static
void
MemByteCalculateZf(
    WmachineState *State,
    reg_t ByteAddress
    )
{
    uint64_t offset;
    reg_t zeroAddr;

    zeroAddr = MEMORY_STATUS(StatusZero);
    TapeWriteBit(State, zeroAddr, 1);

    for (offset = 0; offset < BYTE_WIDTH; ++offset)
    {
        AluBinOpBit(State,
            BinOpAbjunction,
            zeroAddr,
            zeroAddr,
            ByteAddress + offset);
    }
}

static
void
MemByteSubOne(
    WmachineState *State,
    reg_t Dest,
    reg_t Source
    )
{
    uint64_t offset;

    //
    // Clear the carry and write a 1 that we can repeatedly add.
    //
    TapeWriteBit(State, MEMORY_STATUS(StatusCarry), 0);
    TapeWriteBit(State, MEMORY_STATUS(StatusConstantOne), 1);

    for (offset = 0; offset < BYTE_WIDTH; ++offset)
    {
        AluAddBit(State,
            Dest + offset,
            Source + offset,
            MEMORY_STATUS(StatusConstantOne),
            MEMORY_STATUS(StatusCarry));
    }
}

// XXX factor out this so we only have one trie-generating function
static
void
WmachProduceMemTrie(
    WmachineState *State,
    uint32_t Depth,
    uint32_t Prefix
    )
{
    const uint32_t max = (1 << BYTE_WIDTH) - 1;

    if (Prefix <= max && Depth == 0)
    {
        // XXX this is the match case. LD/ST the data.
        reg_t dataTransport = MEMORY_DATA;
        reg_t valueOffset = MEMORY_VALUES + Prefix*WORD_WIDTH;
        TapeCopyWord(State, dataTransport, valueOffset);
        /*
        Tape pos = State->Head;
        TapeWriteMemory(State, pos, Prefix, BYTE_WIDTH);
        IoPutByte(State, pos);
        */
        EmitSimple(Debug);
        EmitHalt();
    }
    else if ((Prefix << Depth) > max)
    {
        //
        // This lower byte of the address is bigger than what is possible.
        // Someone is up to no good. Bail.
        //

        EmitHalt();
    }
    else
    {
        uint64_t prefix1, prefix0;
        char prefix1_label[MAX_LABEL];
        char prefix0_label[MAX_LABEL];
        uint32_t width = BYTE_WIDTH - Depth;
        int64_t savedHead;

        prefix1 = (Prefix << 1) | 1;
        prefix0 = (Prefix << 1) | 0;

        //
        // N.B., We must add 1 to the width as this is the NEXT width since it
        // is the result of having tested the current bit.
        //
        CHECK(GenLabelStatic("mem_prefix",
            width + 1,
            prefix1,
            &prefix1_label[0],
            sizeof(prefix1_label)));
        CHECK(GenLabelStatic("mem_prefix",
            width + 1,
            prefix0,
            &prefix0_label[0],
            sizeof(prefix0_label)));

        //
        // Seek to current bit of the address.
        //
        savedHead = TapeSeek(State, MEMORY_ADDR + OFFSET_BYTE(0) + (Depth - 1));
        EmitFmt("\n");

        //
        // Test the current bit of the address.
        //
        EmitBranch(&prefix1_label[0], &prefix0_label[0]);

        TapeRestore(State, savedHead);
        EmitLabel(&prefix1_label[0]);
        WmachProduceMemTrie(State,
            Depth - 1,
            prefix1);

        TapeRestore(State, savedHead);
        EmitLabel(&prefix0_label[0]);
        WmachProduceMemTrie(State,
            Depth - 1,
            prefix0);
    }
}

static
void
WmachProduceMemTable(
    WmachineState *State
    )
{
    reg_t condAddr = REG_ADDR(Flags) + CondFlag;
    reg_t trueAddr = REG_ADDR(Flags) + TrueFlag;
    reg_t memBase  = REG_ADDR(MemoryBase);
    Tape context;

    //
    // N.B., This is necessary because we can be invoked at arbitrary times.
    // All useres should not call us directly but should instead use
    // WmachMemTransition which handles this interface on their behalf.
    //
    TapeRestore(State, MEM_TRANSITION_HEAD);

    EmitLabel(MEM_DISPATCH_LABEL);

    //
    // Here is the high level idea from Shinichiro Hamaji:
    // We have three levels in a memory hierarchy. The outermost, and largest,
    // is called a region. There are 256 (0xff) of them. Each region consists
    // of a control structure and 256 blocks. Each block contains a control
    // structure (block 0's control structure is aliased with the containing
    // region's control structure) and 256 values. A value is simply 3-bytes of
    // data.
    // To walk along these structures is to read or write memory. To traverse
    // the regions we look at the MSB (0xff0000) of the address. Initially we
    // copy all of the relevant data (address and value if we're writing, just
    // the address if we're reading) into the initial control structure. From
    // here we check if the relevant byte is zero, if so then we know we've
    // arrived. If it is still not zero, we decrement by 1 and store the result
    // in the subsequent region. Each time we encounter a zero we descend to
    // the next containing district. We repeat this process from region to
    // block to value. Once we have performed the requested access we must haul
    // ourselves, and any data we wish to take back with us, back up the memory
    // hierarchy. In this way we pass back across each control structure we
    // initially touched until we are back at the beginning and we then know
    // exactly what the state of the system is and can return to normal
    // operation.
    //

    //
    // Copy over the relevant fields from the instruction.
    //

    TapeCopyBit(State,
        memBase + MEMORY_STATUS(StatusLoadStore),
        REG_ADDR(Flags) + LoadStoreFlag);
    // XXX Both MEMORY_DATA and MEMORY_ADDR are redundant. We should remove
    // MemValue and MemAddress and have the instructions just write to the
    // MEMORY_* locations directly
    // XXX maybe only copy this if we're performing a STORE (as this is the
    // data to write. If this is a load then it will be the data read (after we
    // perform the memop))
    TapeCopyWord(State, memBase + MEMORY_DATA, REG_ADDR(MemValue));
    //
    // This is the address in memory that we will be accessing.
    //
    TapeCopyWord(State, memBase + MEMORY_ADDR, REG_ADDR(MemAddress));

    //
    // Seek to the base of memory, but more importantly to the base of the
    // initial region. Everything is now relatively to this.
    //
    context = TapeSeek(State, memBase);
    EmitFmt("\n");
    TapeRestore(State, 0);

    //
    // Now that we have all of the information we need meaning Ctl of the first
    // block in the first region is initialized. We can slowly crawl along
    // until we hit the desired region corresponding to the address.
    //

    LOG("/* MEMORY ACCESS START */\n");
    {

        //
        // Find which region the data lives in.
        //
        {
            char top[MAX_LABEL];
            char more[MAX_LABEL];
            char out[MAX_LABEL];
            Tape post_loop;

            INIT_LABEL(State, top);
            INIT_LABEL(State, more);
            INIT_LABEL(State, out);

            EmitLabel(&top[0]);
            {
                LOG("/* Test for zero */\n");
                MemByteCalculateZf(State, MEMORY_ADDR + OFFSET_BYTE(1));
                post_loop = TapeSeek(State, MEMORY_STATUS(StatusZero));
                EmitBranch(&out[0], &more[0]);

                EmitLabel(&more[0]);

                //
                // Subtract one from the highest byte of the address.
                //
                LOG("/* Decrement */\n");
                MemByteSubOne(State,
                    MEMORY_ADDR + OFFSET_BYTE(1),
                    MEMORY_ADDR + OFFSET_BYTE(1));

                //
                // Copy over the relevant control structures.
                //
                LOG("/* Copy ctl */\n");
                uint64_t next_region = sizeof(Block)*BYTE_WIDTH;
                TapeCopyBit(State,
                    next_region + MEMORY_STATUS(StatusLoadStore),
                    MEMORY_STATUS(StatusLoadStore));
                TapeCopyWord(State,
                    next_region + MEMORY_DATA,
                    MEMORY_DATA);
                TapeCopyWord(State,
                    next_region + MEMORY_ADDR,
                    MEMORY_ADDR);

                //
                // Seek to the next region and use it as the new basis.
                //
                LOG("/* reset */\n");
                TapeSeek(State, next_region);
                EmitFmt("\n");
                TapeRestore(State, 0);
                EmitBranch(&top[0], &top[0]);
            }

            EmitLabel(&out[0]);
            TapeRestore(State, post_loop);
        }

        {
            //
            // At this point, we know we're in the proper region (i.e., the
            // most-significant byte is accounted for). We must now work on the
            // middle byte.
            //

            WmachProduceMemTrie(State, BYTE_WIDTH, 0);
        }

        // XXX
        EmitSimple(Debug);
        EmitFmt("\n");
    }
    LOG("/* MEMORY ACCESS END */\n");

    //
    // And we are done. We know that we are currently resting at the base of
    // memory so let's restore that context.
    //
    TapeRestore(State, context);

    //
    // This is pretty hacky. Instead of incrementing PC ourselves, we re-use
    // the goto dispatch code. In order to do this, we must set a few flags.
    // Essentially we are tricking it into thinking a failed conditional branch
    // was executed so it will handle the fallthrough. There should probably be
    // a better interface for this.
    //
    TapeWriteBit(State, condAddr, 1);
    TapeWriteBit(State, trueAddr, 0);
    WmachGotoTransition(State);
}


#ifdef DEBUG
static
void
DbgMain(WmachineState *State)
{
    UNREF_PARAM(State);
    Inst test;

    memset(&test, 0, sizeof(test));

    TapeWriteBit(State, REG_ADDR(Flags) + MEMORY_STATUS(LoadStoreFlag), 1);
    TapeWriteWord(State, REG_ADDR(MemValue), 0xa55a);
    // XXX There's a bug where address 0x0000 takes one iteration while 0x0100 does not!
    TapeWriteWord(State, REG_ADDR(MemAddress), 0x0200);
EmitSimple(Debug);
    WmachMemTransition(State);

    WmachProduceMemTable(State);

    EmitLabel(GOTO_DISPATCH_LABEL);

    EmitLabel(FIN_LABEL);
}
#endif

#if 1
static
void
DbgPrintRange(
    WmachineState *State,
    int64_t Base,
    uint64_t Size
    )
{
    int64_t address;

    // No we don't check for overflow in a debug function. Forgive me!
    Size *= WORD_WIDTH;

    // Have mercy
    for (address = Base; address < (int64_t) (Base + Size); address += BYTE_WIDTH)
    {
        if (address == Base)
        {
            EmitFmt("/* DEBUG PRINT: %ld (%ld) */\n", Base, Base / WORD_WIDTH);
        }
        IoPutByte(State, address);
    }
}
#endif

void
target_wm(
    Module *Mod
    )
{
    WmachineState state;
    int previousPc = 0;

(void) DbgPrintRange(NULL, 0, 0); // XXX shut the compiler up. Make sure to remove
    WmachInitialize(&state, Mod);

#ifdef DEBUG
DbgMain(&state);
return;
#endif
WmachProduceGotoTable(NULL); // XXX shut the compiler up. Make sure to remove
MemInitialize(NULL); // XXX shut the compiler up. Make sure to remove
TapePreserve(NULL); // XXX shut the compiler up. Make sure to remove


    //
    // Each basic block expects the head to be at a particular position. Let's
    // seek there now that all the setup is over.
    //
    TapeSeek(&state, BB_HEAD);
    EmitFmt("\n");

    for (;
        WmachMoreInsts(&state);
        WmachNextInst(&state))
    {
        if (state.Current->pc == 0 ||
            (previousPc != state.Current->pc))
        {
            //
            // When we reach this location we are at the border of two basic
            // blocks. We will emit a jmp to the dispatch table (because each
            // basic block returns there to dispatch the next block) as well as
            // the beginning label for the subsequent block.
            //

            char bb_label[MAX_LABEL];

            previousPc = state.Current->pc;

            //
            // We just emitted the last instruction in a basic block. It is
            // likely that this is a jmp, which takes care of itself and cleans
            // up nicely. However it may be an instruction that occurs just
            // before the TARGET of a jmp. In this case, we must clean up on
            // its behalf and set the tape to the expected transition position.
            //
            TapeSeek(&state, BB_HEAD);
            EmitFmt("\n");

            //
            // We emit a label for each basic block. It must be deterministic
            // so that we can branch to it without any context other than its
            // address (what PC would be if it were executing it).
            //

            CHECK(GenLabelBb(state.Current->pc, &bb_label[0], sizeof(bb_label)));
            EmitLabel(&bb_label[0]);

            //
            // N.B., We are emitting a new basic block. The basic block was
            // entered via a high level branch. Due to how WmachGotoTableGen is
            // implemented, it will leave head pointing to the least bit in PC.
            // Because of this, we need to let the basic blocks know exactly
            // where their head actually lies.
            // We are allowed to do this as we ensure this fact everywhere.
            //

            TapeRestore(&state, BB_HEAD);
        }
        else
        {
            EmitFmt("\n");
        }

        WmachProduceInst(&state);
    }
    EmitFmt("\n");

    //
    // This must be the very last thing to be emitted because this is just past
    // the last instruction. If a branch were to land here then it would have
    // no valid instructions to execute. This signals an exit to the
    // interpreter.
    //
    EmitLabel(FIN_LABEL);
}
