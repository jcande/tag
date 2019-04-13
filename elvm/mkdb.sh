#!/bin/sh

rm -f cscope.files

# We care about the order as we want definitions to appear before
# implementations.
find 8cc/ libc/ ir/ target/ -name '*.h' >> cscope.files
find 8cc/ libc/ ir/ target/ -name '*.S' >> cscope.files
find 8cc/ libc/ ir/ target/ -name '*.c' >> cscope.files
find 8cc/ libc/ ir/ target/ -name '*.cc' >> cscope.files

cscope -b -q -k
