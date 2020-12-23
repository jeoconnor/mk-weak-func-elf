#!/usr/bin/env python3

import sys
import subprocess

prelink_args = ["mock-elfpatch", "-w"]
objfiles = [arg for arg in sys.argv[2:] if arg[-2:] == ".o"]

subprocess.run(prelink_args + objfiles)
subprocess.run(sys.argv[1:])


