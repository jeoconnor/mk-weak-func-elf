#!/usr/bin/env python3

import sys
import os
import subprocess

link_preprocess = ["../mock-elfpatch", "-w"]

infix = False
fixargs = []
stdargs = []
for arg in sys.argv[1:]:
    if arg == "--begin-shim":
        infix = True
    elif arg == "--end-shim":
        infix = False
    else:
        if infix:
            fixargs.append(arg)
        else:
            stdargs.append(arg)

if len(fixargs) > 0:
    cmd = [fixargs[0]]
else:
    cmd = ["g++"]

if "-c" in sys.argv:
    pass
else:
    # extract the object files from the command line and
    # preprocess them
    objfiles = [arg for arg in stdargs if arg[-2:] == ".o"]
    subprocess.run(link_preprocess + objfiles)

subprocess.run(cmd + stdargs)


