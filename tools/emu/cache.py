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
        self.data.PushAppendant(data)

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
            print("({}, {}) ".format(sym.symbol, sym.total_size)),
        print("extant: ({}, {})".format(self.data.extant.symbol, self.data.extant.total_size))


"""
When pushing multiple appendants at once, we must solve the packing problem.
Once we get an algorithm for this we can implement it in the interpreter.
"""

class Cache:
    def __init__(self, symbol = None, total_size = 0):
        self.symbol = symbol
        self.total_size = total_size # how many total symbols (including symbol) are accounted for
        self.dirty = (symbol != None)  # whether or not we can flush
    def __str__(self):
        return "{{ sym: {}, total_size: {}, dirty: {} }}".format(self.symbol, self.total_size, self.dirty)

    def clear(self):
        self.symbol = None
        self.dirty = False
        self.total_size = 0

class TagQueue:
    def __init__(self, deletionNumber):
        self.DelNo = deletionNumber
        self.q = []
        self.extant = Cache()

    def Pop(self):
        if self.q:
            symbol = self.q.pop(0)
            assert (total_size % DelNo) == 0
            return (symbol.symbol, symbol.total_size / self.DelNo)
        elif self.extant.total_size >= self.DelNo:
            refs = self.extant.total_size / self.DelNo
            self.extant.total_size %= self.DelNo
            return (self.extant.symbol, refs)
        else:
            print("not enough data in the queue!")
            exit(1)

    # This function can ONLY be called on a DelNo boundary!
    def UpdateCache(self, Sym, Size):
        assert Size <= self.DelNo

        if self.extant.dirty:
            if self.extant.symbol == Sym:
                # symbols match! Keep the gravy train going
                self.extant.total_size += Size
            else:
                # Symbols differ! flush and start new extant
                assert (self.extant.total_size % self.DelNo) == 0
                self.q.append(self.extant)
                self.extant = Cache(Sym, Size)

        else:
            # brand new cache! Ez
            self.extant = Cache(Sym, Size)

    def PushAppendant(self, App):
        if len(App) == 0:
            return

        # this is the residue from previous pushes
        res = self.extant.total_size % self.DelNo
        # this is how many symbols we need to get to a new boundary. If we
        # don't have enough then we just use what we've got
        fill = min(len(App), (self.DelNo - res) % self.DelNo)

        self.extant.total_size += fill

        # are all symbols equivalent?
        homogeneous = (App.count(App[0]) == len(App))

        for i in xrange(fill, len(App), self.DelNo):
            size = min(len(App) - i, self.DelNo)

            self.UpdateCache(App[i], size)

            # we know we're at the last iteration because size < self.DelNo

    def Push(self, App, Count):
        # future optimization, calculate cycles so we don't need this outer
        # loop
        for _ in xrange(Count):
            self.PushAppendant(App)


a = ["a","b","c"]
b = ["a","a","a"]
c = ["t"]
l = "abcdefghijklm"

tag = TagQueue(9)
tag.Push(a, 1)
tag.Push(c, 1)
tag.Push(c, 1)
tag.Push(a, 1)

tag.Push(c, 2)

tag.Push(l, 1)
tag.Push("!"*9, 1)
print map(lambda x: (x.symbol, x.total_size), tag.q)
print tag.extant

test_tag = False
if test_tag:
    b = Tag()
    b.addRule("x", ["x", "x", "x", "x"])
    b.setData(["x", "x"])

    b.run()

if True:
    tag = TagQueue(2)
    tag.Push(c, 3)
    (s, c) = tag.Pop()
    print("popped: {} ({})".format(s, c))
    print("queue: {}".format(map(lambda x: (x.symbol, x.total_size), tag.q)))
    print("extant: {}".format(tag.extant))

