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
# 1
t.addRule("x", ["xa","xa"])                 # status quo
t.addRule("0", ["net","_", "test"])
t.addRule("1", ["net","_", "test", "ya","ya"])
t.addRule("y", ["ya","ya", "ya","ya"])      # double!

# 2
t.addRule("xa", ["xb"])                     # divide by 2
t.addRule("net", ["odd_net","even_net"])    # cast the net! This let's us safely operate on blank (turing) tapes (no xs or ys)
t.addRule("test", ["Odd","Even"])           # We use this to know what value the new head will take
t.addRule("ya", ["yb","yb"])

# 3
t.addRule("xb", ["xc","xc"])
t.addRule("Even", ["0c","0c"])              # even numbers lack a bit in the 1's place
t.addRule("Odd", ["1c","1c"])               # shift out the lower bit!
t.addRule("odd_net", [])                    # keep that odd parity going
t.addRule("even_net", ["pad"])              # swap the parity
t.addRule("yb", ["yc","yc"])

# 4
t.addRule("xc", ["x_new","x_new"])
t.addRule("0c", ["0_new","0_new"])
t.addRule("1c", ["1_new","1_new"])
t.addRule("yc", ["y_new","y_new"])

def test(start, end):
    t.setData(start)
    t.run()

    if t.data != end:
        print "FAIL"
        sys.exit(1)
    else:
        print "Success\n---\n"

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
