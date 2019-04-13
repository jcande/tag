#!/usr/bin/env python

"""
idea: break up the address into 3 sections: 0xAABBCC.
Treat each section like it is a layer in the page tables.
AA is the base of one table. From that, offset BB is the base of the "PTE".
Then CC is the offset into the "page" and contains a 24-bit word that we can
copy.
"""

WORD_WIDTH = 24
DEPTH = 4

bit = 0xdeadbe
bit = bit & ((1 << WORD_WIDTH) - 1)

def gen(depth, prefix, max):
    width = DEPTH - depth
    indent = "    "*(width)

    assert(max < (1<<DEPTH))

    pretty_bin = "{:0{width}b}"
    prefix_fmt = "prefix_" + pretty_bin

    next_1 = (prefix << 1) | 1
    prefix_1 = prefix_fmt.format(next_1, width=width+1)
    next_0 = (prefix << 1) | 0
    prefix_0 = prefix_fmt.format(next_0, width=width+1)

    if depth == 0 and prefix <= max:
        real_label = "_BB_" + pretty_bin.format(prefix, width=width)
        print("{0}jmp {1}, {1}".format(indent, real_label))
    elif (prefix << depth) > max:
        print("{}EXIT".format(indent))
    else:
        print("{}seek PC_base + width".format(indent))
        print("{}jmp {}, {}".format(indent, prefix_1, prefix_0))

        print("{}prefix_{:0{width}b}:".format(indent, next_1, width=width+1))
        gen(depth - 1, next_1, max)

        print("{}prefix_{:0{width}b}:".format(indent, next_0, width=width+1))
        gen(depth - 1, next_0, max)


gen(DEPTH, 0, 3)
