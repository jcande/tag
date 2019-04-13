#/usr/bin/env python

deletion_number = 2
text = "Hello world!\n"

for ch in text:
    n = ord(ch)
    for i in xrange(8):
        if n & (1<<i):
            print "one "*deletion_number
        else:
            print "zero "*deletion_number
