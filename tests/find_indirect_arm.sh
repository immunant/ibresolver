#!/bin/bash

# This script should only be used for creating the callsite lists for arm32 test binaries
arm-linux-gnueabihf-objdump -d $1 | rg "\tbl?x\t" | awk '{print "0x"$1}' | sed 's/://' > $1.txt
