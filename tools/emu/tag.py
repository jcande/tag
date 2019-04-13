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

    def run(self, steps=40):
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
# TODO need to double the xs!
t.addRule("x", ["x1", "x1"])        # status quo
t.addRule("0", ["net", "_", "test"])
t.addRule("1", ["net", "_", "test", "y1", "y1"])
t.addRule("y", ["y1", "y1", "y1", "y1"]) # double!

t.addRule("x1", ["x2"])           # divide by 2
t.addRule("net", ["odd_x", "even_x"])   # cast the net! This let's us safely operate on blank (turing) tapes (no xs or ys)
t.addRule("test", ["Odd", "Even"])      # We use this to know what value the new head will take
t.addRule("y1", ["y2", "y2"])

t.addRule("x2", ["x3", "x3"])
t.addRule("Even", ["03", "03"])           # even numbers lack a bit in the 1's place
t.addRule("Odd", ["13", "13"])            # shift out the lower bit!
t.addRule("odd_x", [])  # keep that odd parity going
t.addRule("even_x", ["pad"])   # swap the parity
t.addRule("y2", ["y3", "y3"])

t.addRule("x3", ["x_new", "x_new"])
t.addRule("03", ["0_new", "0_new"])
t.addRule("13", ["1_new", "1_new"])
t.addRule("y3", ["y_new", "y_new"])

def test(start, end):
    t.setData(start)
    t.run()

    if t.data != end:
        print "FAIL"
        sys.exit(1)
    else:
        print "Success"

test(["0","0"], map(lambda x: x + "_new", ["0","0"]))
test(["0","0", "y","y"], map(lambda x: x + "_new", ["0","0", "y","y", "y","y"]))
test(["0","0", "y","y", "y","y"], map(lambda x: x + "_new", ["0","0", "y","y", "y","y", "y","y", "y","y"]))

test(["1","1"], map(lambda x: x + "_new", ["0","0", "y","y"]))
test(["1","1", "y","y"], map(lambda x: x + "_new", ["0","0", "y","y", "y","y", "y","y"]))
test(["1","1", "y","y", "y","y"], map(lambda x: x + "_new", ["0","0", "y","y", "y","y", "y","y", "y","y", "y","y"]))

test(["x","x", "0","0"], map(lambda x: x + "_new", ["1","1"]))
test(["x","x", "0","0", "y","y"], map(lambda x: x + "_new", ["1","1", "y","y", "y","y"]))
test(["x","x", "0","0", "y","y", "y","y"], map(lambda x: x + "_new", ["1","1", "y","y", "y","y", "y","y", "y","y"]))

test(["x","x", "1","1"], map(lambda x: x + "_new", ["1","1", "y","y"]))
test(["x","x", "1","1", "y","y"], map(lambda x: x + "_new", ["1","1", "y","y", "y","y", "y","y"]))
test(["x","x", "1","1", "y","y", "y","y"], map(lambda x: x + "_new", ["1","1", "y","y", "y","y", "y","y", "y","y", "y","y"]))

test(["x","x", "x","x", "0","0"], map(lambda x: x + "_new", ["x","x", "0","0"]))
test(["x","x", "x","x", "0","0", "y","y"], map(lambda x: x + "_new", ["x","x", "0","0", "y","y", "y","y"]))
test(["x","x", "x","x", "0","0", "y","y", "y","y"], map(lambda x: x + "_new", ["x","x", "0","0", "y","y", "y","y", "y","y", "y","y"]))

test(["x","x", "x","x", "1","1"], map(lambda x: x + "_new", ["x","x", "0","0", "y","y"]))
test(["x","x", "x","x", "1","1", "y","y"], map(lambda x: x + "_new", ["x","x", "0","0", "y","y", "y","y", "y","y"]))
test(["x","x", "x","x", "1","1", "y","y", "y","y"], map(lambda x: x + "_new", ["x","x", "0","0", "y","y", "y","y", "y","y", "y","y", "y","y"]))


print "All tests passed!"
