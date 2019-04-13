from struct import pack

class Tag:
    def __init__(self):
        # symbol is a bytestring and appendant is an array of bytestrings
        # (because symbols can be multibyte)
        self.Rules = {}
        # An array of bytestrings
        self.Queue = []

    def AddRule(self, Key, Value):
        self.Rules[Key[:]] = Value[:]

    def SetQueue(self, Queue):
        self.Queue = Queue[:]

    def Serialize(self, Filename = "tag.bin"):
        ruleCount = len(self.Rules)
        symbolSize = 0
        if (ruleCount > 0):
            key = self.Rules.keys()[0]
            symbolSize = len(key)
        queueSize = len(self.Queue) * symbolSize

        with open(Filename, 'w') as fd:
            # write out header
            fd.write(pack("<Q", ruleCount))
            fd.write(pack("<I", symbolSize))
            fd.write(pack("<I", queueSize))

            for key in self.Rules:
                if (symbolSize != len(key)):
                    raise Exception("Not all symbols are equivalent in length")

                # write rule header
                value = self.Rules[key]
                fd.write(pack("<H", len(value) * symbolSize))

                # write symbol
                fd.write(key)

                # write appendant
                for element in value:
                    if (symbolSize != len(element)):
                        raise Exception("Not all symbols are equivalent in length")
                    fd.write(element)

            # write out queue
            for element in self.Queue:
                if (symbolSize != len(element)):
                    raise Exception("Not all symbols are equivalent in length")
                fd.write(element)


        print(("Rules: {0}\n" +
                "Symbol Size: {1}\n" +
                "Queue Size: {2}\n").format(ruleCount, symbolSize, queueSize))

b = Tag()
b.AddRule("a", ["b", "c"])
b.AddRule("b", ["a"])
b.AddRule("c", ["a", "a", "a"])
b.SetQueue(["a", "a", "a"])
b.Serialize()

"""
x_0 -> x_1 x_1

s0_0 -> s1_1 s1_1
s1_0 -> s1_1 s1_1

y_0 -> y_1 y_1


x_1 -> x_a1 x_a1 xp_a1

s0_1 -> s0_a1 s0_a1
s1_1 -> s1_a1 s1_a1

y_1 -> y_a1 y_a1

x_a1 -> x_b1 x_b1 x_b1
xp_a1 -> x_b1 x_b1

s0_a1 -> t_1
s1_a1 -> x_b1 x_b1 t_1

y_a1 -> y_b1

x_b1 -> x_c1 x_c1

t_1 -> t_a1 tp_a1
y_b1 -> y_c1 y_c1

x_c1 -> x_2 x_2

t_a1 -> s1_2 s1_2
tp_a1 -> x_2 s0_2 s0_2

y_c1 -> y_2 y_2


x_2 -> x_3 x_3

s0_2 -> s1_3 s1_3
s1_2 -> s1_3 s1_3

y_2 -> y_3 y_3


x_3 -> t_3 t_3

s0_3 -> sp0_3
s1_3 -> sp1_3 sp1_3

y_3 -> u_3 up_3

t_3 -> xp_3 xpp_3

sp0_3 -> xp_3 spp0_3 spp0_3
sp1_3 -> spp1_3 spp1_3

u_3 -> yp_3 ypp_3
up_3 -> ypp_3 yp_3

xp_3 -> x_1 x_1
xpp_3 -> x_4 x_4

spp0_3 -> s0_4 s0_4
spp1_3 -> s1_1 s1_1

yp_3 -> y_1 y_1
ypp_3 -> y_4 y_4
"""
