#!/usr/bin/env python

# real shit is under emu

import tag

import fractions
def lcm(a,b): return abs(a * b) / fractions.gcd(a,b) if a and b else 0

"""
When pushing multiple appendants at once, we must solve the packing problem.
Once we get an algorithm for this we can implement it in the interpreter.
"""

global_q = []
def QPush(extant):
    global global_q
    print("Q: {} ({})".format(extant.symbol, extant.refCount))
    global_q.append(extant)

class Appendant:
    def __init__(self, symbols):
        self.symbols = symbols[:]
    def __len__(self):
        return len(self.symbols)

class Extant:
    def __init__(self, symbol, refCount):
        self.symbol = symbol
        self.refCount = refCount

class TagQueue:
    def __init__(self, deletionNumber):
        self.DelNo = deletionNumber
        self.q = []
        self.displacement = 0 # % DelNo
        self.extant = Extant("", 0) # symbol and count
        # or maybe intermediate, transitional

    def PushSymbol(self, Symbol):
        if self.extant.refCount == 0:
            # the queue is empty, this is the first in the cache
            self.extant = Extant(Symbol, 1)
        elif self.extant.symbol != Symbol:
            # we got a new symbol. The extant is now complete.
            self.q.append(extant)
            self.extant = Extant(Symbol, 1)
        else:
            # the extant is still growing in strength
            self.extant.refCount += 1

    def PushAppendant(self, App):
        print("Pushing {}".format(App.symbols))
        if len(App) == 0:
            return

        offset = (self.DelNo - self.displacement) % self.DelNo
        for (i, _) in enumerate(xrange(self.displacement, len(App) - offset, self.DelNo)):
            self.PushSymbol(App.symbols[offset])
            print("{}: {}".format(offset, App.symbols[offset]))
            offset += self.DelNo

        self.displacement = (self.displacement + len(App)) % self.DelNo

    def Push(self, App, Count):
        for _ in xrange(Count):
            self.PushAppendant(App)


class Tag:
    def __init__(self, DeletionNumber):
        # symbol is a bytestring and appendant is an array of bytestrings
        # (because symbols can be multibyte)
        self.Rules = {}
        # An array of bytestrings
        self.Queue = TagQueue(DeletionNumber)

    def AddRule(self, Key, Value):
        self.Rules[Key[:]] = Value[:]

    def SetQueue(self, Queue):
        for symbol in Queue:
            self.Queue.PushSymbol(symbol)


#tag = TagQueue(9)

#a = Appendant(["a","b","c"])
#b = Appendant(["a","a","a"])
#c = Appendant(["t"])
#long = Appendant("abcdefghijklm")

#tag.Push(a, 1)
#tag.Push(c, 1)
#tag.Push(c, 1)
#tag.Push(a, 1)

#tag.Push(c, 2)
#tag.Push(long, 1)
#tag.Push(Appendant("!"*1000), 1)
#print map(lambda x: (x.symbol, x.refCount), global_q)

b = Tag(2)
b.AddRule("x", ["x", "x", "x", "x"])
b.SetQueue(["x", "x"])
