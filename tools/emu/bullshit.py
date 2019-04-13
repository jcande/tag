import sys

# 2 tag only
class Tag:
    def __init__(self):
        self.code = {}
        self.data = []

    def addRule(self, symbol, appendant):
        self.code[symbol] = appendant[:]

    def setData(self, data):
        self.data = data[:]

    def run(self, steps=40000000):
        self.printRules()
        print("\n0: "),
        self.printData()

        for (i, _) in enumerate(xrange(steps), 1):
            symbol = self.data[0]

            appendant = []
            if self.code.has_key(symbol):
                appendant = self.code[symbol]
            else:
                print("Can't find: {}".format(symbol))
                return

            # pop for real
            self.data.pop(0)
            self.data.pop(0)

            self.data.extend(appendant)

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
        for sym in self.data:
            print("{}\t".format(sym)),
        print("")

t = Tag()
t.addRule("x", ["x", "x", "x", "x"])
t.setData(["x", "x"])

t.run()
