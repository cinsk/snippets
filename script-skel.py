#!/usr/bin/env python3

#
# a script template for python3
#

import os
import sys
from getopt import gnu_getopt as getopt

PROGRAM_NAME = os.path.basename(sys.argv[0])
VERSION_STRING = "0.1"



def main():
    opts, args = getopt(sys.argv[1:], 'o:V',
                        ["output=", "help", "version"])
    for o, a in opts:
        if o in ("--help"):
            help_and_exit()
        elif o in ("--version"):
            version_info()
        elif o in ("-o", "--output"):
            output = a
        else:
            error("unknown option -- %s" % o)

    # args is a list of remaining arguments.
    for i, a in zip(range(len(args)), args):
        print("[%d] %s" % (i, a))



def version_info():
    print("%s version %s", PROGRAM_NAME, VERSION_STRING)
    sys.exit(0)

def help_and_exit():
    msg = """Script skeleton

  -o, --output=FILE       output to FILE (default: stdout)

      --help     display this help and exit
      --version  output version information and exit"""
    print(msg)
    sys.exit(0)
    


def error(msg, **kwargs):
    """Print error message and exit the process if needed.

    Keyword arguments:
      prefix  -- if True, add the program name as a prefix  (default: True)
      status  -- if non-zero, call sys.exit()               (default: 1)
    """
    global PROGRAM_NAME
    
    if not "status" in kwargs:
        kwargs["status"] = 1
    if not "prefix" in kwargs:
        kwargs["prefix"] = True
        
    if kwargs["prefix"] and len(PROGRAM_NAME) > 0:
        msg = PROGRAM_NAME + ": " + msg
        
    print(msg, flush = True, file = sys.stderr)
    if kwargs["status"] != 0:
        sys.exit(kwargs["status"])



if __name__ == "__main__":
    main()
