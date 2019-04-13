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
t.addRule("x", ["xa","xa"])         # since x comes before the head, we maintain the status quo for a cycle so that we can be impacted by the parity flip
t.addRule("0", ["net","_", "shift"])
t.addRule("1", ["1a_t", "1a_t"])
t.addRule("y", ["ya_t", "ya_f"])


# 2
t.addRule("xa", ["xb_t","xb_f"])    # ok, now we can participate in the parity flip

t.addRule("1a_t", ["1b_t","1b_t"])
t.addRule("ya_t", ["yb_t","yb_t"])


t.addRule("ya_f", ["yb_f","yb_f"])
t.addRule("net", ["unshift"])
t.addRule("shift", ["0b_f","0b_f"])
t.addRule("xa_f", ["xb_f","xb_f"])


# 3
t.addRule("xb_t", ["x_t","x_t"])
t.addRule("1b_t", ["1_t","1_t"])
t.addRule("yb_t", ["y_t","y_t"])

t.addRule("xb_f", ["x_f","x_f"])
t.addRule("0b_f", ["0_f","0_f"])
t.addRule("yb_f", ["y_f","y_f"])

def test(start, end):
    t.setData(start)
    t.run()

    if t.data != end:
        print "FAIL"
        sys.exit(1)
    else:
        print "Success\n---\n"

test(["0","0"], ["0_f","0_f"])
test(["0","0", "y","y"], ["0_f","0_f", "y_f","y_f"])
test(["0","0", "y","y", "y","y"], ["0_f","0_f", "y_f","y_f", "y_f","y_f"])

test(["1","1"], ["1_t","1_t"])
test(["1","1", "y","y"], ["1_t","1_t", "y_t","y_t"])
test(["1","1", "y","y", "y","y"], ["1_t","1_t", "y_t","y_t", "y_t","y_t"])

test(["x","x", "0","0"], ["x_f","x_f", "0_f","0_f"])
test(["x","x", "0","0", "y","y"], ["x_f","x_f", "0_f","0_f", "y_f","y_f"])
test(["x","x", "0","0", "y","y", "y","y"], ["x_f","x_f", "0_f","0_f", "y_f","y_f", "y_f","y_f"])

test(["x","x", "1","1"], ["x_t","x_t", "1_t","1_t"])
test(["x","x", "1","1", "y","y"], ["x_t","x_t", "1_t","1_t", "y_t","y_t"])
test(["x","x", "1","1", "y","y", "y","y"], ["x_t","x_t", "1_t","1_t", "y_t","y_t", "y_t","y_t"])

test(["x","x", "x","x", "0","0"], ["x_f","x_f", "x_f","x_f", "0_f","0_f"])
test(["x","x", "x","x", "0","0", "y","y"], ["x_f","x_f", "x_f","x_f", "0_f","0_f", "y_f","y_f"])
test(["x","x", "x","x", "0","0", "y","y", "y","y"], ["x_f","x_f", "x_f","x_f", "0_f","0_f", "y_f","y_f", "y_f","y_f"])

test(["x","x", "x","x", "1","1"], ["x_t","x_t", "x_t","x_t", "1_t","1_t"])
test(["x","x", "x","x", "1","1", "y","y"], ["x_t","x_t", "x_t","x_t", "1_t","1_t", "y_t","y_t"])
test(["x","x", "x","x", "1","1", "y","y", "y","y"], ["x_t","x_t", "x_t","x_t", "1_t","1_t", "y_t","y_t", "y_t","y_t"])


print "All tests passed!"
