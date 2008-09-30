#!/usr/bin/env python
# coding: utf-8

# $Id$

#
# hangyou: classic Hangman style game with hints for education.
# Copyright(c) 2008   Seong-Kook Shin <cinsky@gmail.com>
#

import re
import urllib
import sys
import random
import curses
import curses.ascii

URL_WORDS_TODAY = "http://eedic.naver.com/"

TRY_COUNT = 20
HINT_COUNT = 5
WORD_ID_MAX = 32613

trans_table = """\
\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\
\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\
\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\
\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\
\x20\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f\
\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x20\x20\x20\x20\x20\
\x20\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\
\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x20\x20\x20\x20\x20\
\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\
\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\
\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\
\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\
\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\
\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\
\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\
\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"""

gbl_score = 0
gbl_lives = TRY_COUNT
gbl_hints = HINT_COUNT
gbl_solved = 0

gbl_review = dict()

MSG_CHANCE_GAINING = 0.3
MSG_CHANCE_LOSING = 0.5
MSG_CHANCE_DYING = 0.7

COLS = 80
LINES = 24

msgwin = None
boawin = None
defwin = None
scrwin = None

banner_shown = False

gbl_msg_survived = [
    "You survived. \"Damn! I should've prepared tougher question.\"",
    "You survived. \"We'll see. I don't think you're that smart.\"",
    "\"This time, I'll let you free, mortal.\" You survived.",
    "\"A thousand curses upon thee~~.\" You survivied.",
    "\"You bored me, leave me.\" You survived." ]

gbl_msg_died = [
    "Your eyes are burned out. You died.",
    "\"Guard!  Remove this nothing from my sight.\" You died.",
    "You died. \"A pity, why didn't you study more in your lifetime?\"",
    "You died. \"RIP is too much for you.\"",
    "Your body disintegrated.  You died.",
    "Your soul is scorched.  You died.",
    "Your body petrified.  You died.",
    "\"Now, I'm tasting your soul.\"  You died.",
    ]

gbl_msg_dying = [
    "\"Your life force is running out...\"",
    "\"Your coffin is about to arrive.\"",
    "\"Don't worry. I'll raise you later, for my puppet zombie.\"",
    "\"Life down there is not boring, you know.\"",
    "\"It's too late.  Accept what I offered.\"",
    "\"Join me, or die.\"",
    "\"No one can save you except me now.\"",
    ]

gbl_msg_hint = [
    "\"What about `%c'?\"",
    "\"What about `%c'?\"",
    "\"What about `%c'?\"",
    "`%c' can save your ass.",
    "\"You don't deserve for `%c'.\"",
    "\"Worthless! Take this.\"  You got `%c'.",
    "Your body glows for a moment.  You feel `%c'.",
    "You've got a mail. It says `%c'.",
    ]

gbl_msg_losing = [
    "You're one step close to Gehinom.",
    "\"Don't you see that I'm waiting to collect your soul personally?\"",
    "\"AO told me that your soul belongs to me.\"",
    "\"Your afterlife will be much better than now. Interested?\"",
    "\"It's not too late to accept me as thy master.\"",
    "\"Cyric is waiting for thee.\"",
    "\"I'm thinking of selling your soul to Baal.\"",
    "\"Thou hast to know this, thou art too stupid to becoming my minion\"",
    "\"Thy fingers are hunger for my attention.\"",
    "\"What about 'X'?\"",
    "\"What about 'Q'?\"",
    "\"What about 'Z'?\"",
    ]

gbl_msg_gaining = [
    "\"That's the easiest one, you know.\"",
    "\"I prepared plenty of rooms in hell just for you.\"",
    "\"You know, long time ago, I was a mortal like you.\"",
    "\"Don't get excited.  Soon, you'll become one of my minions.\"",
    "\"I'm patient.\"",
    "\"There is no escape! Do not forget that!\"",
    "\"Devious scum!\"",
    "\"I'll not forget this.\"",
    "\"You'll suffer!\"",
    "\"Enough!\"",
    "\"You dare!\"",
    "\"Insolent!\"",
    "\"Ungrateful!\"",
    "\"What about 'X'?\"",
    "\"What about 'Q'?\"",
    "\"What about 'Z'?\"",
    ]
    
                 
                     
class PorterStemmer:
    """Porter Stemming Algorithm
This is the Porter stemming algorithm, ported to Python from the
version coded up in ANSI C by the author. It may be be regarded
as canonical, in that it follows the algorithm presented in

Porter, 1980, An algorithm for suffix stripping, Program, Vol. 14,
no. 3, pp 130-137,

only differing from it at the points maked --DEPARTURE-- below.

See also http://www.tartarus.org/~martin/PorterStemmer

The algorithm as described in the paper could be exactly replicated
by adjusting the points of DEPARTURE, but this is barely necessary,
because (a) the points of DEPARTURE are definitely improvements, and
(b) no encoding of the Porter stemmer I have seen is anything like
as exact as this version, even with the points of DEPARTURE!

Vivake Gupta (v@nano.com)

Release 1: January 2001

Further adjustments by Santiago Bruno (bananabruno@gmail.com)
to allow word input not restricted to one word per line, leading
to:

release 2: July 2008
"""
    def __init__(self):
        """The main part of the stemming algorithm starts here.
        b is a buffer holding a word to be stemmed. The letters are in b[k0],
        b[k0+1] ... ending at b[k]. In fact k0 = 0 in this demo program. k is
        readjusted downwards as the stemming progresses. Zero termination is
        not in fact used in the algorithm.

        Note that only lower case sequences are stemmed. Forcing to lower case
        should be done before stem(...) is called.
        """

        self.b = ""  # buffer for word to be stemmed
        self.k = 0
        self.k0 = 0
        self.j = 0   # j is a general offset into the string

    def cons(self, i):
        """cons(i) is TRUE <=> b[i] is a consonant."""
        if self.b[i] == 'a' or self.b[i] == 'e' or self.b[i] == 'i' or self.b[i] == 'o' or self.b[i] == 'u':
            return 0
        if self.b[i] == 'y':
            if i == self.k0:
                return 1
            else:
                return (not self.cons(i - 1))
        return 1

    def m(self):
        """m() measures the number of consonant sequences between k0 and j.
        if c is a consonant sequence and v a vowel sequence, and <..>
        indicates arbitrary presence,

           <c><v>       gives 0
           <c>vc<v>     gives 1
           <c>vcvc<v>   gives 2
           <c>vcvcvc<v> gives 3
           ....
        """
        n = 0
        i = self.k0
        while 1:
            if i > self.j:
                return n
            if not self.cons(i):
                break
            i = i + 1
        i = i + 1
        while 1:
            while 1:
                if i > self.j:
                    return n
                if self.cons(i):
                    break
                i = i + 1
            i = i + 1
            n = n + 1
            while 1:
                if i > self.j:
                    return n
                if not self.cons(i):
                    break
                i = i + 1
            i = i + 1

    def vowelinstem(self):
        """vowelinstem() is TRUE <=> k0,...j contains a vowel"""
        for i in range(self.k0, self.j + 1):
            if not self.cons(i):
                return 1
        return 0

    def doublec(self, j):
        """doublec(j) is TRUE <=> j,(j-1) contain a double consonant."""
        if j < (self.k0 + 1):
            return 0
        if (self.b[j] != self.b[j-1]):
            return 0
        return self.cons(j)

    def cvc(self, i):
        """cvc(i) is TRUE <=> i-2,i-1,i has the form consonant - vowel - consonant
        and also if the second c is not w,x or y. this is used when trying to
        restore an e at the end of a short  e.g.

           cav(e), lov(e), hop(e), crim(e), but
           snow, box, tray.
        """
        if i < (self.k0 + 2) or not self.cons(i) or self.cons(i-1) or not self.cons(i-2):
            return 0
        ch = self.b[i]
        if ch == 'w' or ch == 'x' or ch == 'y':
            return 0
        return 1

    def ends(self, s):
        """ends(s) is TRUE <=> k0,...k ends with the string s."""
        length = len(s)
        if s[length - 1] != self.b[self.k]: # tiny speed-up
            return 0
        if length > (self.k - self.k0 + 1):
            return 0
        if self.b[self.k-length+1:self.k+1] != s:
            return 0
        self.j = self.k - length
        return 1

    def setto(self, s):
        """setto(s) sets (j+1),...k to the characters in the string s, readjusting k."""
        length = len(s)
        self.b = self.b[:self.j+1] + s + self.b[self.j+length+1:]
        self.k = self.j + length

    def r(self, s):
        """r(s) is used further down."""
        if self.m() > 0:
            self.setto(s)

    def step1ab(self):
        """step1ab() gets rid of plurals and -ed or -ing. e.g.

           caresses  ->  caress
           ponies    ->  poni
           ties      ->  ti
           caress    ->  caress
           cats      ->  cat

           feed      ->  feed
           agreed    ->  agree
           disabled  ->  disable

           matting   ->  mat
           mating    ->  mate
           meeting   ->  meet
           milling   ->  mill
           messing   ->  mess

           meetings  ->  meet
        """
        if self.b[self.k] == 's':
            if self.ends("sses"):
                self.k = self.k - 2
            elif self.ends("ies"):
                self.setto("i")
            elif self.b[self.k - 1] != 's':
                self.k = self.k - 1
        if self.ends("eed"):
            if self.m() > 0:
                self.k = self.k - 1
        elif (self.ends("ed") or self.ends("ing")) and self.vowelinstem():
            self.k = self.j
            if self.ends("at"):   self.setto("ate")
            elif self.ends("bl"): self.setto("ble")
            elif self.ends("iz"): self.setto("ize")
            elif self.doublec(self.k):
                self.k = self.k - 1
                ch = self.b[self.k]
                if ch == 'l' or ch == 's' or ch == 'z':
                    self.k = self.k + 1
            elif (self.m() == 1 and self.cvc(self.k)):
                self.setto("e")

    def step1c(self):
        """step1c() turns terminal y to i when there is another vowel in the stem."""
        if (self.ends("y") and self.vowelinstem()):
            self.b = self.b[:self.k] + 'i' + self.b[self.k+1:]

    def step2(self):
        """step2() maps double suffices to single ones.
        so -ization ( = -ize plus -ation) maps to -ize etc. note that the
        string before the suffix must give m() > 0.
        """
        if self.b[self.k - 1] == 'a':
            if self.ends("ational"):   self.r("ate")
            elif self.ends("tional"):  self.r("tion")
        elif self.b[self.k - 1] == 'c':
            if self.ends("enci"):      self.r("ence")
            elif self.ends("anci"):    self.r("ance")
        elif self.b[self.k - 1] == 'e':
            if self.ends("izer"):      self.r("ize")
        elif self.b[self.k - 1] == 'l':
            if self.ends("bli"):       self.r("ble") # --DEPARTURE--
            # To match the published algorithm, replace this phrase with
            #   if self.ends("abli"):      self.r("able")
            elif self.ends("alli"):    self.r("al")
            elif self.ends("entli"):   self.r("ent")
            elif self.ends("eli"):     self.r("e")
            elif self.ends("ousli"):   self.r("ous")
        elif self.b[self.k - 1] == 'o':
            if self.ends("ization"):   self.r("ize")
            elif self.ends("ation"):   self.r("ate")
            elif self.ends("ator"):    self.r("ate")
        elif self.b[self.k - 1] == 's':
            if self.ends("alism"):     self.r("al")
            elif self.ends("iveness"): self.r("ive")
            elif self.ends("fulness"): self.r("ful")
            elif self.ends("ousness"): self.r("ous")
        elif self.b[self.k - 1] == 't':
            if self.ends("aliti"):     self.r("al")
            elif self.ends("iviti"):   self.r("ive")
            elif self.ends("biliti"):  self.r("ble")
        elif self.b[self.k - 1] == 'g': # --DEPARTURE--
            if self.ends("logi"):      self.r("log")
        # To match the published algorithm, delete this phrase

    def step3(self):
        """step3() dels with -ic-, -full, -ness etc. similar strategy to step2."""
        if self.b[self.k] == 'e':
            if self.ends("icate"):     self.r("ic")
            elif self.ends("ative"):   self.r("")
            elif self.ends("alize"):   self.r("al")
        elif self.b[self.k] == 'i':
            if self.ends("iciti"):     self.r("ic")
        elif self.b[self.k] == 'l':
            if self.ends("ical"):      self.r("ic")
            elif self.ends("ful"):     self.r("")
        elif self.b[self.k] == 's':
            if self.ends("ness"):      self.r("")

    def step4(self):
        """step4() takes off -ant, -ence etc., in context <c>vcvc<v>."""
        if self.b[self.k - 1] == 'a':
            if self.ends("al"): pass
            else: return
        elif self.b[self.k - 1] == 'c':
            if self.ends("ance"): pass
            elif self.ends("ence"): pass
            else: return
        elif self.b[self.k - 1] == 'e':
            if self.ends("er"): pass
            else: return
        elif self.b[self.k - 1] == 'i':
            if self.ends("ic"): pass
            else: return
        elif self.b[self.k - 1] == 'l':
            if self.ends("able"): pass
            elif self.ends("ible"): pass
            else: return
        elif self.b[self.k - 1] == 'n':
            if self.ends("ant"): pass
            elif self.ends("ement"): pass
            elif self.ends("ment"): pass
            elif self.ends("ent"): pass
            else: return
        elif self.b[self.k - 1] == 'o':
            if self.ends("ion") and (self.b[self.j] == 's' or self.b[self.j] == 't'): pass
            elif self.ends("ou"): pass
            # takes care of -ous
            else: return
        elif self.b[self.k - 1] == 's':
            if self.ends("ism"): pass
            else: return
        elif self.b[self.k - 1] == 't':
            if self.ends("ate"): pass
            elif self.ends("iti"): pass
            else: return
        elif self.b[self.k - 1] == 'u':
            if self.ends("ous"): pass
            else: return
        elif self.b[self.k - 1] == 'v':
            if self.ends("ive"): pass
            else: return
        elif self.b[self.k - 1] == 'z':
            if self.ends("ize"): pass
            else: return
        else:
            return
        if self.m() > 1:
            self.k = self.j

    def step5(self):
        """step5() removes a final -e if m() > 1, and changes -ll to -l if
        m() > 1.
        """
        self.j = self.k
        if self.b[self.k] == 'e':
            a = self.m()
            if a > 1 or (a == 1 and not self.cvc(self.k-1)):
                self.k = self.k - 1
        if self.b[self.k] == 'l' and self.doublec(self.k) and self.m() > 1:
            self.k = self.k -1

    def stem(self, p, i = None, j = None):
        """In stem(p,i,j), p is a char pointer, and the string to be stemmed
        is from p[i] to p[j] inclusive. Typically i is zero and j is the
        offset to the last character of a string, (p[j+1] == '\0'). The
        stemmer adjusts the characters p[i] ... p[j] and returns the new
        end-point of the string, k. Stemming never increases word length, so
        i <= k <= j. To turn the stemmer into a module, declare 'stem' as
        extern, and delete the remainder of this file.
        """
        # copy the parameters into statics

        if i == None:
            i = 0
        if j == None:
            j = len(p) - 1
            
        self.b = p
        self.k = j
        self.k0 = i
        if self.k <= self.k0 + 1:
            return self.b # --DEPARTURE--

        # With this line, strings of length 1 or 2 don't go through the
        # stemming process, although no mention is made of this in the
        # published algorithm. Remove the line to match the published
        # algorithm.

        self.step1ab()
        self.step1c()
        self.step2()
        self.step3()
        self.step4()
        self.step5()
        return self.b[self.k0:self.k+1]




def html_detag(string):
    """Remove all HTML tags from the given STRING"""
    re_tag = re.compile("<[^>]*>")
    
    string = string.replace("<br>", "\n", -1)
    while True:
        match = re_tag.search(string)
        if match == None:
            break
        string = string.replace(match.group(), "")

    string = string.replace("&nbsp;", " ", -1)
    return string

def random_msg(msglist):
    return msglist[random.randint(0, len(msglist) - 1)]

def chance(rate):
    if random.random() < rate:
        return True
    return False

def get_word(word_id):
    """Return (WORD DEF) for given WORD_ID"""

    # Normalize WORD_ID
    word_id = (word_id + WORD_ID_MAX) % WORD_ID_MAX
    url = "http://eedic.naver.com/eedic.naver?id=%u" % word_id
    
    re_def = re.compile("(^[0-9][0-9]*\..*$|^[ ]*[A-Z-]+[ ]*)")
    re_word = re.compile("""<span class="hwme2">([^<]*)</span>""")
    
    line = None
    word = None
    
    fd = urllib.urlopen(url)
    while True:
        line = fd.readline()
        if len(line) == 0:
            break
        if line.find("""<span class="hwme2">""") >= 0:
            match = re_word.search(line)
            if match != None:
                word = match.group(1)
            break

    while True:
        line = fd.readline()
        if len(line) == 0:
            break
        if line.find("""<span class="dnum">""") < 0:
            continue
        break
    if line == None:
        return None
    buf = html_detag(line)
    lines = buf.split("\n")
    defs = list()
    idx = 0

    while idx < len(lines):
        match = re_def.match(lines[idx])
        if match != None:
            d = lines[idx + 1].strip() 
            if d != "":
                defs.append(d)
        idx += 1

    if len(defs) == 0:
        return (word, "No hint for now~")
    
    return (word, defs[random.randint(0, len(defs) - 1)])

def get_defs_old(url):
    """Get the definition list from the given URL."""
    re_def = re.compile("(^[0-9][0-9]*\..*$|^[ ]*[A-Z-]+[ ]*)")
    
    line = None
    
    fd = urllib.urlopen(url)

    while True:
        line = fd.readline()
        if len(line) == 0:
            break
        if line.find("""<span class="dnum">""") < 0:
            continue
        break
    if line == None:
        return None
    buf = html_detag(line)
    lines = buf.split("\n")
    defs = list()
    idx = 0

    while idx < len(lines):
        match = re_def.match(lines[idx])
        if match != None:
            d = lines[idx + 1].strip() 
            if d != "":
                defs.append(d)
        idx += 1
        
    return defs

def get_wrong_char(wordset, answer):
    for ch in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
        if answer.has_key(ch) or ch in wordset:
            continue
        return ch.upper()
    return None

def get_right_char(wordset, answer):
    for ch in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
        if answer.has_key(ch) or not ch in wordset:
            continue
        return ch.upper()
    return None
    
def get_today_words():
    """Return (WORD, URL) pairs of today's words"""
    fd = urllib.urlopen(URL_WORDS_TODAY)
    re_word = re.compile("""<a href=([^ ]*) class="text_en">([^<]*)</a>""")
    words = list()
    while True:
        line = fd.readline()
        if len(line) == 0:
            break
        match = re_word.search(line)
        if match == None:
            continue
        url = match.group(1)
        url = "http://eedic.naver.com/%s" % url
        word = match.group(2)

        #print "%s: %s" % (word, url)
        words.append((word, url))
    fd.close()
    return words

def str_partition(s, sep):
    idx = s.find(sep)
    if idx < 0:
        return (s, '', '')
    return (s[0:idx], s[idx], s[idx+1:])
            
def make_desc_list(desc):
    re_delim = re.compile("[^a-zA-Z]+")
    lst = list()
    while True:
        match = re_delim.search(desc)
        if match == None:
            if len(desc) > 0:
                lst.append(desc)
            break

        tok, sep, last = str_partition(desc, match.group())
        if (len(tok) > 0):
            lst.append(tok)
            
        sep = sep.strip()
        if sep != "":
            if sep.find(".") < 0 and len(sep) > 0:
                lst.append(sep)
            else:
                for tok in str_partition(sep, "."):
                    if len(tok) > 0:
                        lst.append(tok)
        desc = last
    return lst
    
def make_quiz(word, desc):
    """Create a quiz description; Substitute all similar occurrences of
    WORD from DESC to blank lines"""

    stemmer = PorterStemmer()

    stem = stemmer.stem(word.lower())

    quiz = ""
    for w in make_desc_list(desc):
        cand = stemmer.stem(w)
        #print "STEM[%s] CAND[%s]" % (stem, cand)
        if stem.lower() == cand.lower():
            quiz += " %s" % ("_" * len(w))
        else:
            if w == "." or w == ":" or w == ";" or w == ",":
                quiz += "%s" % w
            else:
                quiz += " %s" % w

    return quiz[1:]

def answer_board(word, char_dict = None):
    if char_dict == None:
        char_dict = dict()
        for k in word:
            char_dict[k] = True
        
    if len(char_dict) == 0:
        return ("_ " * len(word)).strip()

    result = ""
    for ch in word:
        if char_dict.has_key(ch.upper()):
            result += "%c " % ch.upper()
        else:
            result += "_ "

    return result.strip()

def correct_answer(word, char_dict = None):
    if char_dict == None:
        char_dict = dict()
        
    for ch in word:
        if not char_dict.has_key(ch):
            return False
    return True

def message(screen, pause, *msgs):
    global scrwin, COLS
    
    screen.erase()
    y = 0;
    nmsg = len(msgs)
    for msg in msgs:
        if pause and y + 1 == nmsg:
            msg += " --MORE--"
        screen.addstr(y, 0, msg)
        y += 1
    screen.refresh()
    scrwin.move(0, COLS - 1)
    scrwin.refresh()

    if pause:
        ch = screen.getch()
        return screen.getch()

def ask(screen, answer, *msgs):
    while True:
        message(screen, False, *msgs)
        ch = screen.getch()
        if ch != None and curses.ascii.isascii(ch):
            if answer.find(chr(ch)) >= 0:
                return chr(ch)
            if ch == curses.ascii.ESC:
                return None
        curses.flash()

def draw_score_board(screen):
    global gbl_lives, gbl_score, gbl_hints
    
    screen.erase()
    screen.addstr(0, 0, "HP: %d\tScore: %d\tChances: %d" % \
                  (gbl_lives, gbl_score, gbl_hints))
    screen.refresh()
    
def draw_answer_board(screen, word, char_dict = None):
    board = answer_board(word, char_dict)
    x = (curses.COLS - len(board)) / 2
    y = 1
    screen.addstr(y, x, board)

    if char_dict != None:
        tried = ""
        for k in char_dict.keys():
            tried += " %c" % k.upper()

        tried = "[%s]" % tried[1:]
    
        y += 2
        x = (curses.COLS - len(tried)) / 2
        screen.addstr(y, x, tried)
    
    screen.refresh()
    

def register_review(word):
    global gbl_review

    if gbl_review.has_key(word):
        gbl_review[word] += 1
    else:
        gbl_review[word] = 1
        
def game(msgwin, boawin, defwin, scrwin, word, desc):
    global banner_shown, gbl_score, gbl_lives
    global gbl_msg_gaining, gbl_msg_dying, gbl_msg_losing
    global gbl_msg_died, gbl_msg_survived, gbl_msg_hint
    global gbl_solved, gbl_hints
    
    answers = dict()
    trycount = 0

    word = word.upper()
    wordset = set(word)

    wdef = make_quiz(word, desc)
    defwin.addstr(0, 0, wdef)
    defwin.refresh()
    
    draw_answer_board(boawin, word, answers)
    draw_score_board(scrwin)

    
    if not banner_shown:
        message(msgwin, False,
                "Welcome to the Pandemonium.  Solve it or die.")
        banner_shown = True
        
    while True:
        ch = msgwin.getch()
        message(msgwin, False)
        if ch == curses.ascii.ESC:
            break

        char = None
        if curses.ascii.isalpha(ch):
            char = chr(ch).upper()
            #stdscr.addstr(1, 0, "ch = %c" % char)

            if answers.has_key(char):
                message(msgwin, False,
                        "You already tried '%c'. Try another." % char)
                scrwin.move(0, curses.COLS - 1)
                scrwin.refresh()
                continue

            answers[char] = True
            draw_answer_board(boawin, word, answers)
            
            if char in wordset:
                if chance(MSG_CHANCE_GAINING):
                    message(msgwin, False, random_msg(gbl_msg_gaining))
                gbl_score += 1
            else:
                if gbl_lives == 1 and chance(MSG_CHANCE_DYING):
                    message(msgwin, False, random_msg(gbl_msg_dying))
                elif chance(MSG_CHANCE_LOSING):
                    message(msgwin, False, random_msg(gbl_msg_losing))
                gbl_lives -= 1
                
            draw_score_board(scrwin)
        elif curses.ascii.isascii(ch) and chr(ch) == '?':
            #print "ch: ", ch
            #print "WORDSET: ", wordset
            #print "ANSWER: ", answers
            register_review(word)
            if gbl_hints > 0:
                message(msgwin, False,
                        random_msg(gbl_msg_hint) % \
                        get_right_char(wordset, answers))
                gbl_hints -= 1
            else:
                message(msgwin, False, "You ran out of chances")
        else:
            message(msgwin, False, "Only alphabet keys are allowed")

        if correct_answer(word, answers):
            gbl_solved += 1
            gbl_hints += 1
            ret = ask(msgwin, "yn", random_msg(gbl_msg_survived),
                      "Play more? (y/n) ")
            if ret == 'y':
                return True
            else:
                return False

        if gbl_lives <= 0:
            register_review(word)
            draw_answer_board(boawin, word)
            ret = ask(msgwin, "yn", random_msg(gbl_msg_died),
                      "Play more? (y/n) ")
            if ret == 'y':
                gbl_score = 0
                gbl_lives = TRY_COUNT
                gbl_hints = HINT_COUNT
                gbl_solved = 0
                return True
            else:
                return False

        scrwin.move(0, curses.COLS - 1)
        scrwin.refresh()

def show_copyright(win):
    msgs = [
        "HangYou version 0.1",
        "Copyright (c) 2008    Seong-Kook Shin <cinsky@gmail.com>",
        "With the courtesy of Naver, words information from Collins Cobuild",
        ]
    y = 0
    for msg in msgs:
        win.addstr(y, 0, msg)
        y += 1
    win.refresh()
        
def main():
    global msgwin, boawin, scrwin, defwin, gbl_solved, gbl_score, gbl_review
    global COLS, LINES
    stdscr = curses.initscr()

    COLS = curses.COLS
    LINES = curses.LINES
    
    stdscr.leaveok(1)
    
    curses.noecho()
    curses.cbreak()

    msgwin = curses.newwin(2, curses.COLS - 4, 0, 2)
    boawin = curses.newwin(5, curses.COLS, 2, 0)
    scrwin = curses.newwin(1, curses.COLS, curses.LINES - 2, 0)
    notwin = curses.newwin(4, curses.COLS - 4, curses.LINES - 6, 2)
    defwin = curses.newwin(curses.LINES - 13, curses.COLS - 4, 7, 2)

    word = "hello"

    show_copyright(notwin)
    
    while True:
        msgwin.erase()
        boawin.erase()
        defwin.erase()
        scrwin.erase()

        message(msgwin, False, "Selecting a word...")
        while True:
            # Currently, multi-word dictionary entry is not supported.
            # Thus, loop until one-word dictionary entry is found
            wid = random.randint(0, WORD_ID_MAX)
            #wid = 29052
            wdef = get_word(wid)
            if wdef != None and len(make_desc_list(wdef[0])) == 1:
                #print wid, wdef
                break
        message(msgwin, False)
            
        if not game(msgwin, boawin, defwin, scrwin, wdef[0], wdef[1]):
            break
            
    curses.endwin()

    print "You solved %d question(s) and got %d scores in your lifetime." \
          % (gbl_solved, gbl_score)

    if len(gbl_review) > 0:
        print "From your history, you may not know these words:\n"
        for k, v in gbl_review.iteritems():
            print " %s (%d) http://eedic.naver.com/search.naver?query=%s" % \
                  (k, v, k.lower())
    print ""
    return

    words = get_today_words()

    for w in words:
        print "WORD[%s]: %s" % (w[0], w[1])
        defs = get_defs_old(w[1])

        for d in defs:
            print "\t", make_quiz(w[0], d)
            

if __name__ == '__main__':
    try:
        main()
    except Exception, e:
        curses.endwin()
        print "error: exception occurred: ", e
        sys.exit(1)
