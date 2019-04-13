#!/usr/bin/env python


memory = "\x80\xff"

head = 0
load_addr_offset = 0 # 32-bits wide
load_data_offset = load_addr_offset + 32 # 8-bits wide
memory_offset = load_data_offset + 8 # 2**n BYTES wide


class B_Machine:
    def __init__(self):
        self.head = 0
        self.head_stack = []
        self.code = []

    def __enter__(self):
        self.head_stack.append(self.head)

    def __exit__(self, type, value, traceback):
        self.head = self.head_stack.pop()

    def newContext(self):
        # so we can do: with machine.newContext():
        return self

    def seek(self, new_pos):
        if new_pos > self.head:
            self.code.append(">" * (new_pos - self.head))
        else:
            self.code.append("<" * (self.head - new_pos))
        self.head = new_pos

    def seekRel(self, pos):
        if pos > 0:
            self.seek(self.head + pos)
        else:
            self.seek(self.head - pos)

    def appendCode(self, insns):
        # assert(insns is a list)
        self.code += insns


def mkCopy(src, dst):
    machine = B_Machine()

    shared_labels = []
    for bit in xrange(8):
        shared_labels.append("check_bit_{}".format(bit))
    shared_labels.append("finished")

    for bit in xrange(8):
        set_label = "copy_set_{}".format(bit)
        unset_label = "copy_unset_{}".format(bit)

        machine.appendCode([ "{}:".format(shared_labels[bit]) ])
        machine.seek(src + bit)
        machine.appendCode([ "jmp {}, {}".format(set_label, unset_label) ])

        with machine.newContext():
            machine.appendCode([ "{}:".format(set_label) ])
            machine.seek(dst + bit)
            machine.appendCode([ "+" ])
            machine.seek(src + bit)
            machine.appendCode([ "jmp {0}, {0}".format(shared_labels[bit + 1]) ])

        with machine.newContext():
            machine.appendCode([ "{}:".format(unset_label) ])
            machine.seek(dst + bit)
            machine.appendCode([ "-" ])
            machine.seek(src + bit)
            machine.appendCode([ "jmp {0}, {0}".format(shared_labels[bit + 1]) ])

    machine.appendCode([ "{}:".format(shared_labels[-1]) ])

    return machine.code


def flatten(S):
    if S == []:
        return S
    elif isinstance(S[0], list):
        return flatten(S[0]) + flatten(S[1:])
    else:
        return S[:1] + flatten(S[1:])

def as_bits(segment):
    bits = []

    for ch in segment:
        ch = ord(ch)
        for i in xrange(8):
            mask = 1 << (8 - i - 1)
            if (ch & mask) != 0:
                bits.append(1)
            else:
                bits.append(0)

    return bits

used_address_bits = 8
def mkSchemeHelper(prefix, depth, address_bits_left):
    def pretty(tag):
        return bin(tag)[2:].zfill(depth)

    if address_bits_left == 0:
        # At this point, the head is pointing just past the address region and
        # into the start of load result region
        #return [as_bits(segment[0]), as_bits(segment[1])]
        code  = ["/* deref {:b} */\n".format(prefix)]
        code += ["/* skip over unused addr bits */" + (">" * (32 - depth)) + '\n' ]
        code += mkCopy(prefix, 0)
        return code

    depth += 1
    address_bits_left -= 1

    unset_prefix = (prefix << 1) | 0
    pretty_unset = pretty(unset_prefix)
    set_prefix   = (prefix << 1) | 1
    pretty_set   = pretty(set_prefix)

    code = "jmp addr_{}, addr_{}".format(pretty_set, pretty_unset)
    set_code  = [ "addr_{}:".format(pretty_set)
                , ">"
                ]

    unset_code  = [ "addr_{}:".format(pretty_unset)
                  , ">"
                  ]

    return [code,
               [set_code, mkSchemeHelper(set_prefix, depth, address_bits_left)],
               [unset_code, mkSchemeHelper(unset_prefix, depth, address_bits_left)]]


def mkScheme(address_bits):
    return flatten(mkSchemeHelper(0, 0, address_bits))


def setPattern(pattern):
    code = ""
    for bit in pattern:
        if bit == 1:
            code += '+'
        else:
            code += '-'
        code += '>'
    code += '<'*len(pattern)
    return code

with open("deref_test.wm", 'w') as f:
    for line in mkScheme(4):
        f.write(line + '\n')
