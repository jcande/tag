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
    def __init__(self, symbol, rem, empty = True):
        self.symbol = symbol
        self.remaining = rem	# this is the remaining length of the appendant
        self.empty = empty
        self.refCount = 0
    def __str__(self):
        return "{{ sym: {}, rem: {}, empty: {}, ref: {} }}".format(self.symbol, self.remaining, self.empty, self.refCount)

    def ref(self):
        self.refCount += 1

    def copy(self):
        new = Extant(self.symbol[:], self.rem, False)
        new.refCount = self.refCount

    def clear(self):
        self.symbol = None
        self.empty = True
        self.refCount = 0
        self.remaining = 0

class TagQueue:
    def __init__(self, deletionNumber):
        self.DelNo = deletionNumber
        self.q = []
        self.displacement = 0 # % DelNo
        self.extant = Extant("", 0) # symbol and count
        # or maybe intermediate, transitional
        # count should imply multiples of DelNo, right? It seems like we are overloading refCount in PushSymbol/Pop

    def Pop(self):
        if not self.q:
            # the list is empty but we may have an extant!
            if self.extant.refCount > 0:
                if self.extant.remaining >= self.DelNo:
                    # clean! The queue is now completely empty
                    symbol = self.extant.copy()
                    self.extant.clear()
                    print("empty queue, no displacement: {}".format(symbol.symbol))
                    return (symbol.symbol, symbol.refCount)
                else:
                    # messy. The queue now has < DelNo elements in it
                    symbol = self.copy()
                    self.extant.refCount = 0
                    print("empty queue, some displacement: {}".format(symbol.symbol))
                    return (symbol.symbol, symbol.refCount)
            else:
                print("Not enough data in the queue!")
                print self.extant
                exit(0)
        else:
            print("nonempty queue: {}".format(self.q[0].symbol))
            symbol = self.q.pop(0)
            return (symbol.symbol, symbol.refCount)

    def PushSymbol(self, Symbol, Rem):
        if self.extant.empty:
            # the queue is empty, this is the first in the cache
            self.extant = Extant(Symbol, Rem, False)
            print("brand new symbol: {}".format(Symbol))
        else:
            self.extant.remaining += Rem
            if self.extant.remaining > self.DelNo:
                # the extant is still growing in strength
                self.extant.refCount += 1
            print("returning symbol: {}, {}".format(self.extant.symbol, self.extant.refCount))
            if self.extant.symbol != Symbol:
                # we got a new symbol. The extant is now complete.
                self.q.append(self.extant)
                self.extant = Extant(Symbol, Rem, False)
                print("brand new symbol: {}".format(Symbol))

    def PushAppendant(self, App):
        if len(App) == 0:
            return

        offset = (self.DelNo - self.displacement) % self.DelNo
        self.displacement = (self.displacement + len(App)) % self.DelNo

        if len(App) < offset:
            return

        # this needs to update the extant! We are re-implementing a numbering system that "rolls over" onto refCount
        normalized_len = (len(App) - offset)
        iterations = normalized_len / self.DelNo
        if (normalized_len % self.DelNo) != 0:
            iterations += 1

        for i in xrange(iterations):
            # XXX send more data to PushSymbol so it can know if the symbol is
            # "full" or still requires being fed. Maybe offset % DelNo?
            self.PushSymbol(App[offset], len(App) - offset)
            print("{}: {} {}".format(i, offset, App[offset]))
            offset += self.DelNo

    def Push(self, App, Count):
        # future optimization, calculate cycles so we don't need this outer
        # loop
        for _ in xrange(Count):
            self.PushAppendant(App)


a = ["a","b","c"]
b = ["a","a","a"]
c = ["t"]
l = "abcdefghijklm"

"""
tag = TagQueue(9)
tag.Push(a, 1)
tag.Push(c, 1)
tag.Push(c, 1)
tag.Push(a, 1)

tag.Push(c, 2)

tag.Push(l, 1)
tag.Push("!"*1000, 1)
print map(lambda x: (x.symbol, x.refCount), tag.q)
"""

test_tag = False
if test_tag:
    b = Tag()
    b.addRule("x", ["x", "x", "x", "x"])
    b.setData(["x", "x"])

    b.run()

if True:
    tag = TagQueue(2)
    tag.Push(c, 2)
    (s, c) = tag.Pop()
    print("popped: {} ({})".format(s, c))
    print("queue: {}".format(map(lambda x: (x.symbol, x.refCount), tag.q)))
    print("extant: {}, {}, {}".format(tag.extant.symbol, tag.extant.refCount, tag.extant.empty))

