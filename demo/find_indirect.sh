#!/bin/bash
objdump -d $1 | rg "(jmp |call)   [^0-9,a-f].*$" | awk '{print "0x"$1}' | sed 's/://' > $1.txt
