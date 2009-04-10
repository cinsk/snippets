#!/usr/bin/env python
# -*- coding: utf-8 -*-
# -*- mode: python -*-

#
# gettube: download video file(s) from youtube.com
# Copyright (c) 2009  Seong-Kook Shin <cinsky@gmail.com>
#

# $Id$

import getopt
import tempfile
import codecs
import sys
import urllib
import os
import re
import getopt

fmtinfo = {
     "22" : "30 fps, video: H.264/AVC1, audio: AAC v4 LC stereo, MP4 v2",
     "35" : "30 fps, video: H.264/AVC1, audio: AAC stereo, FLV",
     "18" : "30 fps, video: H.264/AVC1, audio: AAC stereo, MP4 v2",
      "6" : "30 fps, video: FLV1(S H.263), audio: MP3 mono, FLV",
     "34" : "30 fps, video: H.264, audio: AAC, FLV",
      "5" : "30 fps, video: FLV1(S H.263), audio: MP3 mono, FLV",
"default" : "30 fps, video: FLV1(S H.263), audio: MP3 mono, FLV",
}
    
VERSION_STRING = "gettube version 0.1"
PROGRAM_NAME = os.path.basename(sys.argv[0])

def error(status, msg):
    global PROGRAM_NAME
    sys.stderr.write("%s: %s\n" % (PROGRAM_NAME, msg))

    if status > 0:
        sys.exit(status)
        
def get_file(url):
    fd = urllib.urlopen(url)
    buf = fd.read(-1)
    fd.close()

    fpair = tempfile.mkstemp(".html", "url")
    os.write(fpair[0], buf)
    os.close(fpair[0])
    return fpair[1]

def get_context(vid):
    f = get_file("http://youtube.com/watch?v=%s" % vid)

    fd = codecs.open(f, "rb", "UTF-8")

    re_arg = re.compile(u"""swfArgs *= *({[^}]*});$""")
    re_title = re.compile(u"""title=([^']*)';$""")
    
    d = dict()
    s = ""
    title = ""
    
    while True:
        line = fd.readline()
        if len(line) == 0:
            break

        match = re_arg.search(line)
        if match != None:
            s = match.group(1)
            break

    while True:
        line = fd.readline()
        if len(line) == 0:
            break

        match = re_title.search(line)
        if match != None:
            title = match.group(1)
            break

    fd.close()
    os.remove(f)

    try:
        d = eval(s, {"null": ""})
    except:
        error(0, "parse error on swfArgs")
        d = dict()
        
    d["title"] = title
    
    return d

hook_once = 0

def rephook(block_count, block_size, total_size):
    global hook_once;

    if hook_once == 0:
        sizestr = "unknown";
        if total_size > 1024:
            total_size = total_size / 1024;
            if total_size > 1024:
                total_size = total_size / 1024;
                sizestr = "%u MB" % total_size;
            else:
                sizestr = "%u KB" % total_size;
        elif total_size > 0:
            sizestr = "%u byte(s)" % total_size;

        sys.stderr.write("Total size: %s\n" % sizestr);
        sys.stderr.write("block size: %d\n" % block_size);
        sys.stderr.write("Progress: 000 0");
        hook_once = 1;
    else:
        sys.stderr.write("\b\b\b\b\b%3d %%" % \
                         (block_count * block_size * 100 / total_size));

def get_video(vid, outfile = None):
    url = "http://www.youtube.com/get_video?video_id=%s&t=%s"
    mydict = get_context(vid)

    print "Token: %s" % mydict["t"]
    print "Title: %s" % mydict["title"]
    #print "Fmt: %s" % mydict["fmt_map"]

    usefmt = False

    if not mydict.has_key("t"):
        error(0, "parse error: 't' not found.")
        return False
    
    if not mydict.has_key("title"):
        error(0, "warning: title not found.")
        mydict["title"] = "youtube"
        
    fmt = "default"
    ext = "flv"
    try:
        fmt = mydict["fmt_map"].split("/")[0]
        if fmt == "22" or fmt == "18":
            ext = "mp4"

        try:
            f = int(fmt)
        except ValueError:
            fmt = "default"
            
    except KeyError:
        pass

    if fmtinfo.has_key(fmt):
        print "format (guessed): %s" % fmtinfo[fmt]
    else:
        print "format: %s" % mydict["fmt_map"]

    if outfile:
        if outfile.endswith(ext):
            filename = outfile
        else:
            filename = "%s.%s" % (outfile, ext)
    else:
        filename = "%s.%s" % (mydict["title"], ext)

    print "filename: %s" % filename

    url = url % (vid, mydict["t"])
    if fmt != "default":
        url = "%s&fmt=%s" % (url, fmt)

    #print "url: %s" % url
    
    urllib.urlretrieve(url, filename, rephook)        

def usage():
    global PROGRAM_NAME
    
    print """\
usage: %s [OPTION...] VIDEO-ID...
Download video file(s) from YouTube.com

  -o, --output=FILE    override the default output filename
  -h, --help           display this help and exit
      --version        output version information and exit

It is recommended not to use filename extension on FILE.  This program
will add the appropriate filename extension such as '.flv' or '.mp4'.

Warning: Do not use `-o' option with multiple VIDEO-IDs; if you do, it
will overrite previous video file with the current one, so that only
the last video will survive.

Report bugs to <cinsky@gmail.com>.""" % PROGRAM_NAME
        
def main():
    global hook_once, program_name;
    sys.stdout = codecs.getwriter("UTF-8")(sys.__stdout__)

    try:
        opts, args = getopt.getopt(sys.argv[1:], "ho:",
                                   ["help", "version", "output="])
    except getopt.GetoptError:
        error(1, "invalid argument")

    outfile = None
    
    for o, a in opts:
        if o in ("-h", "--help"):
            usage()
            sys.exit()
        elif o in ("--version"):
            print VERSION_STRING
            sys.exit()
        elif o in ("-o", "--output"):
            outfile = a
        else:
            error(1, "invalid argument")

    if len(args) == 0:
        error(0, "argument(s) required.")
        error(1, "Try `--help' for more information.")
        
        
    for f in args:
        get_video(f, outfile)
        sys.stderr.write("\n");
        hook_once = 0;
    

if __name__ == "__main__":
    main()


