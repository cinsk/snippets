#!/usr/bin/env python
# coding: utf-8

# $Id$

#
# mkworddb.py:  Create English word database from http://eedic.naver.com/
# Copyright (c) 2008  Seong-Kook Shin <cinsky@gmail.com>
#

import urllib
import re
import codecs
import sys
import getopt
import os
import sqlite3

WORD_ID_MAX = 32613
WORDDIR = "words.d"
OUTPUT = "eedic.db"

mkworddb_version = "$Revision$"

def error(status, errnum, message, *args):
    """Print a given message into STDERR.  If STATUS is nonzero, the program will be terminated with STATUS."""
    if sys.argv[0] != "":
        sys.stderr.write("%s: " % sys.argv[0])
        
    sys.stderr.write(message % args)
    try:
        sys.stderr.write(": %s" % errno.errorcode[errnum])
    except KeyError:
        pass
    sys.stderr.write("\n")
    
    if status != 0:
        sys.exit(status)
    
def decode_unicode_entity(s):
    """Decode Unicode HTML Entity such as "&#244;" or "&#x00e9;\""""
    re_dec_ent = re.compile("""&#([0-9]+);""")
    re_hex_ent = re.compile("""&#x([0-9a-fA-F]+);""")

    while True:
        match = re_dec_ent.search(s)
        if match == None:
            break
        code = int(match.group(1))
        high = (code >> 8) & 0xff
        low  = code & 0xff
        uni = unicode(chr(high) + chr(low), "UTF-16BE")
        s = s.replace(match.group(0), uni)

    while True:
        match = re_hex_ent.search(s)
        if match == None:
            break
        code = int(match.group(1), 16)
        high = (code >> 8) & 0xff
        low  = code & 0xff
        uni = unicode(chr(high) + chr(low), "UTF-16BE")
        s = s.replace(match.group(0), uni)
    return s
        
def get_html(dirname, word_id):
    try:
        url = "http://eedic.naver.com/eedic.naver?id=%u" % word_id
        rfd = urllib.urlopen(url)
        name = os.path.join(dirname, "%d.html" % word_id)
        wfd = open(name, "w")

        sys.stdout.write("Saving %s..." % name)
        sys.stdout.flush()
        
        wfd.write(rfd.read(-1))
        wfd.close()
        rfd.close()
        sys.stdout.write("done\n")
    except IOError, (errno, strerror):
        error(0, errno, "")
    
def get_star(word_id):
    """Return (WORD_ID, WORD, STAR) for given WORD_ID
where WORD_ID is the argument, WORD is a word string in unicode,
and STAR is between 0-5."""
    re_word = re.compile("""<span id="entry"[^>]*>([^<]*)</span>""")
    re_star = re.compile("""img_star_([1-5]).gif""")
    url = "http://eedic.naver.com/eedic.naver?id=%u" % word_id

    fd = urllib.urlopen(url)

    word = None
    star = 0
    
    while True:
        line = fd.readline()
        if len(line) == 0:
            break
        if line.find("""<span id=\"entry\"""") >= 0:
            match = re_word.search(line)
            if match != None:
                word = decode_unicode_entity(match.group(1))
                break
            
    while True:
        line = fd.readline()
        if len(line) == 0:
            break
        if line.find("""<div class="star">""") >= 0:
            match = re_star.search(line)
            if match != None:
                try:
                    star = int(match.group(1))
                except ValueError:
                    star = -1
                break

    fd.close()
    return (word_id, word, star)

def get_def_from_html(dirname, word_id):
    re_word = re.compile("""<span id="entry"[^>]*>([^<]*)</span>""")
    re_star = re.compile("""img_star_([1-5]).gif""")
    re_sound = re.compile("""http://[^"]*\.mp3""") #"
    
    fname = os.path.join(dirname, "%d.html" % word_id)
    
    if not os.access(fname, os.R_OK):
        return False

    fd = codecs.open(fname, "r", "cp949")

    word = None
    defblk = None
    sound = None
    star = -1
    
    while True:
        line = fd.readline()

        if len(line) == 0:
            break

        if line.find("source_contents") >= 0:
            index = line.find("value=\"")
            start = end = 0
            if index >= 0:
                start = index + 7       # len("value=\")
            end = line.rfind("\">")
            if end >= 0:
                defblk = line[start:end]
            break

    while True:
        line = fd.readline()

        if len(line) == 0:
            break

        if line.find("<span id=\"entry\"") >= 0:
            match = re_word.search(line)
            if match != None:
                word = match.group(1).strip()
            break

    while True:
        line = fd.readline()
        if len(line) == 0:
            break
        if line.find("""<div class="star">""") >= 0:
            match = re_star.search(line)
            if match != None:
                print "STAR!!!"
                try:
                    star = int(match.group(1))
                except ValueError:
                    star = -1
                break
        
    while True:
        line = fd.readline()
        if len(line) == 0:
            break
        if line.find("""class="play">""") >= 0:
            match = re_sound.search(line)
            if match != None:
                print "SOUND!!!"
                sound = match.group(0)
                break

    fd.close()
    return (word, star, sound, defblk)
            
def main():
    sys.stdout = codecs.getwriter('UTF-8')(sys.stdout)

    try:
        opts, args = getopt.getopt(sys.argv[1:], "o:Or:S",
                                   ["help", "range=", "overwrite", \
                                    "saveonly", "output=", "version"])
    except getopt.GetoptError:
        # print help information and exit:
        sys.stderr.write("%s: invalid option\n" % sys.argv[0])
        sys.stderr.write("Try `%s --help' for more information.\n" % \
                         sys.argv[0])
        sys.exit(1)

    fd = None
    output = None
    overwrite = False
    wrange = word_range()
    saveonly = False
    
    for o, a in opts:
        if o in ("--help"):
            usage()
            sys.exit(0)
        elif o in ("--version"):
            print version_string()
            sys.exit(0)
        elif o in ("-o", "--output"):
            output = a
        elif o in ("-r", "--range"):
            wrange = word_range(a)
        elif o in ("-O", "--overwrite"):
            overwrite = True
        elif o in ("-S", "--saveonly"):
            saveonly = True

    if wrange == None or len(wrange) != 2:
        sys.stderr.write("mkworddb: wrong RANGE-SPEC")
        sys.exit(1)

    if saveonly:
        if output == None:
            output = WORDDIR

        if os.access(output, os.F_OK):
            if not os.path.isdir(output):
                error(1, 0, "%s is not a directory" % output)
        else:
            os.makedirs(output, 0755)
    else:
        if overwrite:
            fd = codecs.open(output, "w", "UTF-8")
        else:
            fd = codecs.open(output, "a", "UTF-8")
    
    for wid in range(wrange[0], wrange[1]):
        if saveonly:
            get_html(output, wid)
        else:
            ret = get_star(wid)

            if ret == None or len(ret) != 3:
                print "# WID(%d) error" % wid
                fd.write("# WID(%d) error\n" % wid)
            else:
                print "%d||%s||%d" % (ret[0], ret[1], ret[2])
                fd.write("%d||%s||%d\n" % (ret[0], ret[1], ret[2]))

    if fd != None:
        fd.close()

def word_range(rangespec = None):
    """Return (begin, end) pair from the given RANGE_SPEC string.
RANGE_SPEC is one of "BEGIN-END", "BEGIN-", or "-END", "NUMBER"."""
    global WORD_ID_MAX

    if rangespec == None:
        return (1, WORD_ID_MAX + 1)
    
    ret = rangespec.partition("-")

    try:
        if ret[0] == "" or int(ret[0]) < 1:
            begin = 1
        else:
            begin = int(ret[0])

        if ret[1] == "":
            end = begin
        elif ret[2] == "" or int(ret[2]) > WORD_ID_MAX:
            end = WORD_ID_MAX + 1
        else:
            end = int(ret[2]) + 1
    except ValueError:
        return None
    
    if begin > end:
        return (end, begin)
    return (begin, end)

def usage():
    msg = """\
Create English word database from http://eedic.naver.com/
usage: %s [OPTION...]

  -o, --output=FILE   set output database name to FILE
                      If `-S' specified, this option
                      specifies the directory name.
  
  -O, --overwrite     remove previous database file if any
  -r, --range=RANGE   limit word range to RANGE

  -S, --saveonly      save all word entries (html) into
                      the directory specified in `-o'.
                      
  --help              show this help messages and exit
  --version           show version string and exit

  RANGE should be one of START-END or -END or START- form.
"""
    print msg % sys.argv[0]

def version_string():
    global mkworddb_version
    re_ver = re.compile("([0-9.]+)")

    match = re_ver.search(mkworddb_version)
    if match:
        s = "mkworddb revision %s" % match.group(1)
    else:
        s = "mkworddb revision ALPHA"
    return s
    
    
if __name__ == '__main__':
    main()
