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

(WC_ADJ,                                # adjective
 WC_ADV,                                # adverb
 WC_AUX,                                # auxiliary verb
 WC_COLOUR,                             # colour word
 WC_COMB,                               # combining form
 WC_CONJ,                               # conjunction
 WC_CONVENTION,                         # convention
 WC_DET,                                # determiner
 WC_EXCLAM,                             # exclamation
 WC_FRACTION,                           # fraction
 WC_LINK,                               # see WC_V_LINK
 WC_MODAL,                              # modal verb
 WC_N_COUNT,                            # count noun
 WC_N_COUNT_COLL,                       # collective count noun
 WC_N_FAMILY,                           # family noun
 WC_N_IN_NAMES,                         # noun in names
 WC_N_MASS,                             # mass noun
 WC_N_PLURAL,                           # plural noun
 WC_N_PROPER,                           # proper noun
 WC_N_PROPER_COLL,                      # collective proper noun
 WC_N_PROPER_PLURAL,                    # plural proper noun
 WC_N_SING,                             # singular noun
 WC_N_SING_COLL,                        # collective singular noun
 WC_N_TITLE,                            # title noun
 WC_N_UNCOUNT,                          # uncount noun
 WC_N_UNCOUNT_COLL,                     # collective uncount noun
 WC_N_VAR,                              # variable noun
 WC_N_VAR_COLL,                         # coolective variable noun
 WC_N_VOC,                              # vocative noun
 WC_NEG,                                # negative
 WC_NUM,                                # number
 WC_ORD,                                # ordinal
 WC_PASSIVE,                            # see WC_V_PASSIVE
 WC_PHRASAL_VERB,                       # phrasal verb
 WC_PHRASE,                             # phrase
 WC_PREDET,                             # predeterminer
 WC_PREFIX,                             # prefix
 WC_PREP,                               # preposition
 WC_PREP_PHRASE,                        # phrasal preposition
 WC_PRON,                               # pronoun
 WC_QUANT,                              # quantifier
 WC_QUANT_PLURAL,                       # plural quantifier
 WC_QUEST,                              # question word
 WC_RECIP,                              # See WC_V_RECIP
 WC_SOUND,                              # sound noun
 WC_SUFFIX,                             # suffix
 WC_VERB,                               # verb
 WC_V_LINK,                             # link verb
 WC_V_PASSIVE,                          # passive verb
 WC_V_RECIP,                            # reciprocal verb
 WC_V_RECIP_PASSIVE ) = range(51)       # passive reciprocal verb

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

def parse_definition(defstr):
    pass
    
def get_star(fd):
    """Return (WORD_ID, WORD, DEFINITION, STAR) from given file, FD.
where WORD is a word string in unicode,
and STAR is between 0-5."""
    re_wid = re.compile("""value="?([0-9]+)"?>""")
    re_word = re.compile("""<span id="entry"[^>]*>([^<]*)</span>""")
    re_star = re.compile("""img_star_([1-5]).gif""")

    word_id = None
    word = None
    star = 0
    defblk = ""

    while True:
        # Find the word id
        line = fd.readline()
        if len(line) == 0:
            break

        if line.find("source_no") >= 0:
            match = re_wid.search(line)
            if match != None:
                word_id = int(match.group(1))
                break
            
    while True:
        # Find the definition block
        line = fd.readline()
        if len(line) == 0:
            break

        if line.find("source_contents") >= 0:
            begin = line.find("value=\"")
            if begin < 0:
                continue
            begin += 7 # Add the length of "value=\""
            end = line.rfind("\">")
            if end < 0:
                continue
            defblk = line[begin:end]
            break
    
    while True:
        # Find the name of the word
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
    return (word_id, word, defblk, star)

def parse_wordhtml(dirname, word_id):
    name = os.path.join(dirname, "%d.html" % word_id)

    if not os.access(fname, os.R_OK):
        return False


    
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
        opts, args = getopt.getopt(sys.argv[1:], "o:Or:SW:",
                                   ["help", "range=", "overwrite", \
                                    "saveonly", "output=", \
                                    "wordid=", "version"])
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
    wid = None
    
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
        elif o in ("-W", "--wordid"):
            wid = int(a)
            
    if wrange == None or len(wrange) != 2:
        sys.stderr.write("mkworddb: wrong RANGE-SPEC")
        sys.exit(1)


    if wid != None:
        if output == None:
            output = WORDDIR
        fname = os.path.join(output, "%d.html" % wid)
        fd = codecs.open(fname, "r", "cp949")
        ret = get_star(fd)

        print "name: %s" % ret[1]
        print ""
        print "id: %d" % ret[0]
        print ""
        print "def: %s" % ret[2]
        print ""
        print "star: %d" % ret[3]
        sys.exit(0)

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
            url = "http://eedic.naver.com/eedic.naver?id=%u" % word_id
            fd = urllib.urlopen(url)

            ret = get_star(fd)

            if ret == None or len(ret) != 3:
                print "# WID(%d) error" % wid
                fd.write("# WID(%d) error\n" % wid)
            else:
                print "%d||%s||%d" % (ret[0], ret[1], ret[3])
                fd.write("%d||%s||%d\n" % (ret[0], ret[1], ret[3]))

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

  -c, --create        create the initial database
                      
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
