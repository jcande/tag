import sys


import fractions
def lcm(a,b): return abs(a * b) / fractions.gcd(a,b) if a and b else 0

# 2 tag only
class Tag:
    def __init__(self):
        self.code = {}
        self.data = TagQueue(2)

    def addRule(self, symbol, appendant):
        self.code[symbol] = appendant[:]

    def setData(self, data):
        for symbol in data:
            self.data.PushSymbol(symbol)

    def run(self, steps=20):
        self.printRules()
        print("\n0: "),
        self.printData()

        for (i, _) in enumerate(xrange(steps), 1):
            (symbol, count) = self.data.Pop()

            appendant = []
            if self.code.has_key(symbol):
                appendant = self.code[symbol]
            else:
                print("Can't find: {}".format(symbol))
                return

            self.data.Push(appendant, count)

            print("{}: ".format(i)),
            self.printData()

    def printRule(self, symbol, appendant):
        print("{}\t-> ".format(symbol)),
        for sym in appendant:
            print("{} ".format(sym)),
        print("")

    def printRules(self):
        for sym in self.code.keys():
            appendant = self.code[sym]
            self.printRule(sym, appendant)

    def printData(self):
        for sym in self.data.q:
            print("({}, {}) ".format(sym.symbol, sym.refCount)),
        print("extant: ({}, {})".format(self.data.extant.symbol, self.data.extant.refCount))


"""
When pushing multiple appendants at once, we must solve the packing problem.
Once we get an algorithm for this we can implement it in the interpreter.
"""

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

    def Pop(self):
        if not self.q:
            # the list is empty but we may have an extant!
            if self.displacement == 0:
                # clean!
                symbol = self.extant
                self.extant = Extant("", 0)
                print("empty queue, no displacement: {}".format(symbol.symbol))
                return (symbol.symbol, symbol.refCount)
            elif self.extant.refCount > 1:
                # messy
                symbol = self.extant
                symbol.refCount - 1
                self.extant.refCount = 1
                print("empty queue, some displacement: {}".format(symbol.symbol))
                return (symbol.symbol, symbol.refCount)
            else:
                print("Not enough data in the queue!")
                exit(0)

        else:
            print("nonempty queue: {}".format(self.q[0].symbol))
            symol = self.q.pop(0)
            return (symbol.symbol, symbol.refCount)

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
        if len(App) == 0:
            return

        offset = (self.DelNo - self.displacement) % self.DelNo
        for (i, _) in enumerate(xrange(self.displacement, len(App) - offset, self.DelNo)):
            self.PushSymbol(App[offset])
            #print("{}: {}".format(offset, App[offset]))
            offset += self.DelNo

        self.displacement = (self.displacement + len(App)) % self.DelNo

    def Push(self, App, Count):
        for _ in xrange(Count):
            self.PushAppendant(App)


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

b = Tag()
b.addRule("x", ["x", "x", "x", "x"])
b.setData(["x", "x"])

b.run()
