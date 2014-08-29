#!/usr/bin/env python2.7
# -*-python-*-

"""Simple Consistent Hash Implementation
"""

#
# This implementation is for self-study only; very poor implementation
#

#
# After execution, serveral dot files are created.  You can convert to
# PNG files using:
#
# $ for f in chash*.dot; do circo -Tpng -o${f/.dot/.png} $f; done
#

import hashlib
import sys
import itertools
import random

class chash:
    NODE_SUM, NODE_NAME = range(2)
    
    def __init__(self):
        self.nodes = list()
        self.hashs = dict()
        self.count = 0
        
    def hash(self, obj):
        m = hashlib.sha256()
        m.update(str(obj))
        return int(m.hexdigest(), 16)

    def hashlabel(self, obj):
        m = hashlib.sha256()
        m.update(str(obj))
        return m.hexdigest()[:6]
        
    def add(self, node):
        nodepair = (self.hash(node), str(node))
        self.nodes.append(nodepair)
        self.nodes.sort()
        #self.nodes.sort(lambda x, y: int((x[0] - y[0]) % sys.maxint))
        self.hashs[str(node)] = dict()

        # TODO: rebalance keys in the next elem of NODEPAIR
        nth = self.nodes.index(nodepair)
        dst_pair = self.nodes[(nth + 1) % len(self.nodes)]

        dst = self.hashs[dst_pair[chash.NODE_NAME]]
        self.hashs[dst_pair[chash.NODE_NAME]] = dict()
        
        print "rebalancing: %s needs rebalancing" % dst_pair[chash.NODE_NAME]

        for k, v in dst.iteritems():
            print "\trebalancing %s" % k
            self[k] = v

    def remove(self, node):
        nodepair = (self.hash(node), str(node))
        idx = self.nodes.index(nodepair)

        src_pair = self.nodes[idx]
        dst_pair = self.nodes[(idx + 1) % len(self.nodes)]
        src = self.hashs[src_pair[chash.NODE_NAME]]
        dst = self.hashs[dst_pair[chash.NODE_NAME]]

        print "rebalancing: move all pairs from %s to %s" % \
          (src_pair[chash.NODE_NAME], dst_pair[chash.NODE_NAME])
        
        del self.hashs[self.nodes[idx][chash.NODE_NAME]]
        del self.nodes[idx]

        for k, v in src.iteritems():
            print "\trebalancing %s" % k
            dst[k] = v
                
    def findnode(self, obj):
        node = None
        val = self.hash(obj)
        for elm in self.nodes:
            if val < elm[chash.NODE_SUM]:
                node = elm
                break
        return node if node else self.nodes[chash.NODE_SUM]

    def __getitem__(self, key):
        nodepair = self.findnode(key)
        return self.hashs[nodepair[chash.NODE_NAME]][key]
    
    def __setitem__(self, key, value):
        nodepair = self.findnode(key)
        print "add to %s" % nodepair[chash.NODE_NAME]
        self.hashs[nodepair[chash.NODE_NAME]][key] = value

    def __delitem__(self, key):
        nodepair = self.findnode(key)
        del self.hashs[nodepair[chash.NODE_NAME]][key]
        
    def iteritems(self):
        for h in self.hashs.itervalues():
            for k, v in h.iteritems():
                yield (k, v)
                
    def itervalues(self):
        for h in self.hashs.itervalues():
            for v in h.itervalues():
                yield v

    def iterkeys(self):
        for h in self.hashs.itervalues():
            for k in h.iterkeys():
                yield k

    def __iter__(self):
        return itertools.chain.from_iterable([i for i in c.hashs.itervalues()])

    def iterhashes(self):
        for hsh, name in self.nodes:
            yield (name, self.hashs[name])
            
    def nthhash(self, index):
        pair = self.nodes[index]
        return self.hashs[pair[chash.NODE_NAME]]

    def nthname(self, index):
        return self.nodes[index][chash.NODE_NAME]
            
    def gdump(self, name):
        with open("chash-%s.dot" % str(name), "w") as f:
            kid = 0
            hid = 0
            f.write("digraph chash {\n")

            prev = None
            for name, hsh in self.iterhashes():
                if prev:
                    f.write("%s -> hash%d;\n" % (prev, hid))
                f.write("hash%d [ label=\"%s\\n%s\" style=filled fillcolor=\"yellow\" ];\n" %
                         (hid, name, self.hashlabel(name)))
                prev = "hash%d" % hid
                hid += 1
                for k, v in hsh.iteritems():
                    f.write("  key%d [ label=\"[%s] = %s\\n%s\" shape=box ];\n" %
                            (kid, str(k), str(v), self.hashlabel(k)))
                    f.write("%s -> key%d;\n" % (prev, kid))
                    prev = "key%d" % kid
                    kid += 1

            if hid > 0:
                f.write("%s -> hash%d;\n" % (prev, 0))
            
            print "prev: %s" % prev
            # prevname = "hash%s" % self.nthname(-1)
            # for name, hsh in self.iterhashes():
            #     #f.write("%s -> %s;\n" % (prevname, name))
            #     f.write("hash%s [ label=\"%s\" shape=box ];\n" % (name, name))
            #     for k, v in hsh.iteritems():
            #         f.write("%s -> key%d;\n" % (prevname, kid))
            #         f.write("  key%d [ label=\"%s = %s\" ];\n" %
            #                 (kid, str(k), str(v)))
            #         prevname = "key%d" % kid
            #         kid += 1
            #     #f.write("%s -> %s;\n" % 
            #     prevname = "hash%s" % name
                
            f.write("}\n")

if __name__ == "__main__":
    c = chash()
    c.add("hash1")
    c.gdump("0")
    for i in xrange(10):
        c[i] = int(random.random() * 100)
    c.gdump("1")

    c.add("hash2")
    c.gdump("2")
    c.add("hash3")
    c.gdump("3")

    for i in xrange(10,40):
        c[i] = int(random.random() * 100)
    c.gdump("4")
    c.add("hash4")
    c.add("hash5")
    c.add("hash6")
    c.gdump("5")
    c.remove("hash3")
    c.gdump("6")
    
    print "%s" % str(c.hashs)
