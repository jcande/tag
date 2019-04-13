#!/usr/bin/env python

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

print('\n'.join(mkCopy(0, 8)))
