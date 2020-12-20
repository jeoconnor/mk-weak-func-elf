#!/usr/bin/env python3

print("hello world");

import sys
import os
import subprocess

print(sys.argv)

fixopts = []
stdopts = []

infix = False

for arg in sys.argv[1:]:
    if arg == "--begin-fix":
        infix = True
    elif arg == "--end-fix":
        infix = False
    else:
        if infix:
            fixopts.append(arg)
        else:
            stdopts.append(arg)

cmd = [fixopts[0]]
cmd = cmd + stdopts

print("fix: ", fixopts[1:])
print("cmd: ", cmd)

            
subprocess.Popen(fixopts[1:])
subprocess.Popen(cmd)


