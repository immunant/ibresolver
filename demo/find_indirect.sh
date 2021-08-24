#!/bin/bash

# This script should only be used for creating the callsite lists for x86 test binaries
objdump -d $1 | rg "(jmp |call)   [^0-9,a-f].*$" | awk '{print "0x"$1}' | sed 's/://' > $1.txt
