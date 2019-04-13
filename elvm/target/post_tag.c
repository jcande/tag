#include <ir/ir.h>
#include <target/util.h>

//
// Problems:
// A nice layout for initialized data (should probably be "to the left" of the
// starting tape). For example, maybe d0 can be -1 (byte), d1 can be -2, etc.
// See bf_init_state for specifics.
// 

//
// Memory Layout:
// The post-tag system has an infinite tape in both directions. This tape is
// zeroed by default. E.g.,
// ...0...0...0...
//        ^
//        |
//        +-< tape head
//
// We carve up this memory in a few ways. First, the memory "to the left" of
// the tape corresponds exlusively to initialized data (i.e., constants in the
// program source like strings). If we were to number the number each cell of
// the tape, with 0 being the initial cell that the head points to, this
// section would BEGIN at -1, continuing to -2, -3, etc.
//
// For the nonnegative cells, we use specific offsets as aliases for specific
// data represented in the EIR. Our scheme is very similar to the
// brainfuck-backend.
//
// For an exhaustive list of these magic address we have:
// PC: 0..3, 
//
// This leaves cells x, x+1, x+2, ... to correspond to dynamic memory like the stack or heap.
//
// Our code must keep track of how it has moved the head so that it is able to
// properly address the cells as our layout is purely logical and does not exist
// in the post-tag system.

//
// This is the entry point/callback for this code and is invoked automatically
// by the compiler.
//
void target_post_tag(Module* module) {
    uint32_t i;

    printf("HELLO THERE\n");

    // Data is compile-time data (constants, strings, etc)

    Data *data;
    i = 0;
    for (data = module->data; data != NULL; data = data->next) {
        fprintf(stderr, "d%u: %d\n", i++, data->v);
    }


    Inst *insn;
    i = 0;
    for (insn = module->text; insn != NULL; insn = insn->next) {
        fprintf(stderr, "i%u: ", i++);
        dump_inst(insn);
    }
}
