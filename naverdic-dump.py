#!/usr/bin/env python
# coding: utf-8

# $Id$

#
# naverdic-dump.py:  Dump naver English dictionary
# Copyright (c) 2010  Seong-Kook Shin <cinsky@gmail.com>
#

import urllib
import re
import codecs
import sys
import getopt
import os
import sqlite3
import errno

EEDIC_DICT_URL = "http://eedic2009.naver.com/eedic.naver?mode=word&id=%d"
EEDIC_DICT_MAX = 32698
EEDIC_THES_URL = "http://eedic2009.naver.com/thesaurus.naver?id=%d"
EEDIC_THES_MAX = 7131

DT_DICT, DT_THES = range(2)


DS_URL, DS_IDMAX, DS_FNAME = range(3)
dictionary_schema = {
    DT_DICT : (EEDIC_DICT_URL, EEDIC_DICT_MAX, "w-%d.html" ),
    DT_THES : (EEDIC_THES_URL, EEDIC_THES_MAX, "t-%d.html" ),
    }
    

out_directory = "./dict.d"
force_overwrite = False
dictionary_type = DT_DICT
file_range = None

def error(status, errnum, message, *args):
    """error(status, errnum, message, *args) -> Print an error message.

error() prints a message into STDERR.  If `status' is nonzero,
the program will be terminated with `status'."""
    if sys.argv[0] != "":
        sys.stderr.write("%s: " % sys.argv[0])
        
    sys.stderr.write(message % args)
    try:
        if type(errnum) == int:
            sys.stderr.write(": {0}".format(errno.errorcode[errnum]))
        else:
            sys.stderr.write(": {0}".format(errnum))
    except KeyError:
        pass
    sys.stderr.write("\n")
    
    if status != 0:
        sys.exit(status)
    
def dic_id_range(rangespec = None, dictype = DT_DICT):
    """dic_id_range(rangespec, dictype) -> Return (begin, end) pair

`RANGESPEC' is one of "BEGIN-END", "BEGIN-", or "-END", "NUMBER" form
where BEGIN, END, NUMBER is an integer value.

`DICTYPE' is one of DT_DICT or DT_THES; DT_DICT indicates to use
a word dictionary, and DT_THES indicates to use a thesaurus."""
    global dictionary_schema

    if rangespec == None or rangespec == "":
        return (1, dictionary_schema[dictype][DS_IDMAX])
    
    ret = rangespec.partition("-")

    try:
        if ret[0] == "" or int(ret[0]) < 1:
            begin = 1
        else:
            begin = int(ret[0])

        if ret[1] == "":
            end = begin
        elif ret[2] == "" or int(ret[2]) > dictionary_schema[dictype][DS_IDMAX]:
            end = dictionary_schema[dictype][DS_IDMAX]
        else:
            end = int(ret[2])
    except ValueError:
        return None
    
    if begin > end:
        return (end, begin)
    return (begin, end)
    
def get_html(dirname, word_id, dictype = DT_DICT):
    name = ""
    url = ""
    #print "get_html({0}, {1}, {2})".format(dirname, word_id, dictype)
    try:
        url = dictionary_schema[dictype][DS_URL] % word_id
        fd_in = urllib.urlopen(url)
        name = os.path.join(dirname,
                            dictionary_schema[dictype][DS_FNAME] % word_id)

        fd_out = open(name, "w")

        sys.stdout.write("saving %s..." % name)
        sys.stdout.flush()

        fd_out.write(fd_in.read(-1))
        fd_out.close()
        fd_in.close()

        sys.stdout.write("done\n")
    except IOError as err:
        error(0, 0, "In saving {0}:".format(name))
        error(0, 0, "From {0}:".format(url))
        error(0, err.errno, "I/O Error: {0}".format(err.strerror))
        
def usage():
    print """\
Downloads Naver's dictionary/thesaurus html files
usage: {0} OPTIONS...

  -O DIR        Set output directory to DIR.

  -t TYPE       Set the database type.
                TYPE is one of "dict" or "thes"
                If you want to download everything, you may need to
                invoke this script twice; one for "dict", and another
                for "thes".

  -w            Use the word dictionary (default)
  -t            Use the thesaurus
  
  -f            Force overwrite.
  
  -r RANGE      limits a range of word html files.
                RANGE should be one of "START-END", "-END", "START-", "NUMBER"
                form.
""".format(sys.argv[0])
    
def main():
    global out_directory, force_overwrite, dictionary_type, file_range
    
    sys.stdout = codecs.getwriter('UTF-8')(sys.stdout)

    user_range = None
    
    try:
        opts, args = getopt.getopt(sys.argv[1:], "o:ft:r:",
                                   ["help", "version", "output=",
                                    "force", "type", "range="])
    except getopt.GetoptError:
        # print help information and exit:
        sys.stderr.write("%s: invalid option\n" % sys.argv[0])
        sys.stderr.write("Try `%s --help' for more information.\n" % \
                         sys.argv[0])
        sys.exit(1)

    for o, a in opts:
        if o in ("--help"):
            usage()
            sys.exit(0)
        elif o in ("--version"):
            version()
            sys.exit(0)
        elif o in ("-o", "--output"):
            out_directory = a
        elif o in ("-f", "--force"):
            force_overwrite = True
        elif o in ("-t"):
            if a[0] == "d":
                dictionary_type = DT_DICT
            elif a[0] == "t":
                dictionary_type = DT_THES
            else:
                error(1, 0, "invalid database type, {0}".format(a))
                
        elif o in ("-r", "--range"):
            user_range = a

    file_range = dic_id_range(user_range, dictionary_type)

    if os.access(out_directory, os.F_OK):
        if not os.path.isdir(out_directory):
            error(1, 0, "%s is not a directory" % out_directory)
    else:
        os.makedirs(out_directory, 0755)

    if len(args) != 0:
        error(1, 0, "wrong number of arguments")
        
    #print "args: {0}".format(args)
    #print "out_directory: {0}".format(out_directory)
    #print "force_overwrite: {0}".format(force_overwrite)
    #print "dict type: {0}".format(dictionary_type)
    #print "range: {0}".format(file_range)

    for w in range(file_range[0], file_range[1]):
        get_html(out_directory, w, dictionary_type)
        #pass

if __name__ == '__main__':
    main()
