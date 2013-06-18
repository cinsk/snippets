/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  linux/include/linux/rbtree.h

  To use rbtrees you'll have to implement your own insert and search cores.
  This will avoid us to use callbacks and to drop drammatically performances.
  I know it's not the cleaner way,  but in C (not in C++) to get
  performances and genericity...

  Some example of insert and search follows here. The search is a plain
  normal search over an ordered tree. The insert instead must be implemented
  in two steps: First, the code must insert the element in order as a red leaf
  in the tree, and then the support library function rb_insert_color() must
  be called. Such function will do the not trivial work to rebalance the
  rbtree, if necessary.

-----------------------------------------------------------------------
static inline struct page * rb_search_page_cache(struct inode * inode,
                                                 unsigned long offset)
{
        struct rb_node * n = inode->i_rb_page_cache.rb_node;
        struct page * page;

        while (n)
        {
                page = rb_entry(n, struct page, rb_page_cache);

                if (offset < page->offset)
                        n = n->rb_left;
                else if (offset > page->offset)
                        n = n->rb_right;
                else
                        return page;
        }
        return NULL;
}

static inline struct page * __rb_insert_page_cache(struct inode * inode,
                                                   unsigned long offset,
                                                   struct rb_node * node)
{
        struct rb_node ** p = &inode->i_rb_page_cache.rb_node;
        struct rb_node * parent = NULL;
        struct page * page;

        while (*p)
        {
                parent = *p;
                page = rb_entry(parent, struct page, rb_page_cache);

                if (offset < page->offset)
                        p = &(*p)->rb_left;
                else if (offset > page->offset)
                        p = &(*p)->rb_right;
                else
                        return page;
        }

        rb_link_node(node, parent, p);

        return NULL;
}

static inline struct page * rb_insert_page_cache(struct inode * inode,
                                                 unsigned long offset,
                                                 struct rb_node * node)
{
        struct page * ret;
        if ((ret = __rb_insert_page_cache(inode, offset, node)))
                goto out;
        rb_insert_color(node, &inode->i_rb_page_cache);
 out:
        return ret;
}
-----------------------------------------------------------------------
*/

#ifndef RBTREE_H__
#define RBTREE_H__

#include <stddef.h>


#ifndef offsetof
#ifdef GCC
#define offsetof(st, m) __builtin_offsetof(st, m)
#else
#define offsetof(st, m) ((size_t)(&((st *)0)->m))
#endif
#endif

#ifndef container_of
#define container_of(ptr, type, member) ({ \
                const typeof( ((type *)0)->member ) *__mptr = (ptr); \
                (type *)( (char *)__mptr - offsetof(type,member) );})
#endif


struct rb_node
{
        unsigned long  rb_parent_color;
#define RB_RED          0
#define RB_BLACK        1
        struct rb_node *rb_right;
        struct rb_node *rb_left;
} __attribute__((aligned(sizeof(long))));
    /* The alignment might seem pointless, but allegedly CRIS needs it */

struct rb_root
{
        struct rb_node *rb_node;
        size_t nmodified;
        size_t size;
};


#define rb_parent(r)   ((struct rb_node *)((r)->rb_parent_color & ~3))
#define rb_color(r)   ((r)->rb_parent_color & 1)
#define rb_is_red(r)   (!rb_color(r))
#define rb_is_black(r) rb_color(r)
#define rb_set_red(r)  do { (r)->rb_parent_color &= ~1; } while (0)
#define rb_set_black(r)  do { (r)->rb_parent_color |= 1; } while (0)

static inline void rb_set_parent(struct rb_node *rb, struct rb_node *p)
{
        rb->rb_parent_color = (rb->rb_parent_color & 3) | (unsigned long)p;
}
static inline void rb_set_color(struct rb_node *rb, int color)
{
        rb->rb_parent_color = (rb->rb_parent_color & ~1) | color;
}

#define RB_ROOT (struct rb_root) { NULL, 0, 0, }
#define rb_entry(ptr, type, member) container_of(ptr, type, member)

#define RB_EMPTY_ROOT(root)     ((root)->rb_node == NULL)
#define RB_EMPTY_NODE(node)     (rb_parent(node) == node)
#define RB_CLEAR_NODE(node)     (rb_set_parent(node, node))

static inline void rb_init_node(struct rb_node *rb)
{
        rb->rb_parent_color = 0;
        rb->rb_right = NULL;
        rb->rb_left = NULL;
        RB_CLEAR_NODE(rb);
}

extern void rb_insert_color(struct rb_node *, struct rb_root *);
extern void rb_erase(struct rb_node *, struct rb_root *);

typedef void (*rb_augment_f)(struct rb_node *node, void *data);

extern void rb_augment_insert(struct rb_node *node,
                              rb_augment_f func, void *data);
extern struct rb_node *rb_augment_erase_begin(struct rb_node *node);
extern void rb_augment_erase_end(struct rb_node *node,
                                 rb_augment_f func, void *data);

/* Find logical next and previous nodes in a tree */
extern struct rb_node *rb_next(const struct rb_node *);
extern struct rb_node *rb_prev(const struct rb_node *);
extern struct rb_node *rb_first(const struct rb_root *);
extern struct rb_node *rb_last(const struct rb_root *);

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
extern void rb_replace_node(struct rb_node *victim, struct rb_node *new,
                            struct rb_root *root);

static inline void rb_link_node(struct rb_node * node, struct rb_node * parent,
                                struct rb_node ** rb_link)
{
        node->rb_parent_color = (unsigned long )parent;
        node->rb_left = node->rb_right = NULL;

        *rb_link = node;
}

#define RB_SEARCH(root, usertype, handle, userkey, compare, key)        ({ \
    usertype *ret = 0;                                                  \
    struct rb_node *node = (root)->rb_node;                             \
    while (node) {                                                      \
      usertype *data = container_of(node, usertype, handle);            \
      int result = compare((key), data->userkey);                       \
      if (result < 0)                                                   \
        node = node->rb_left;                                           \
      else if (result > 0)                                              \
        node = node->rb_right;                                          \
      else {                                                            \
        ret = data;                                                     \
        break;                                                          \
      }                                                                 \
    }                                                                   \
    ret;                                                                \
  })

#define RB_INSERT(root, usertype, userkey, handle, compare, data)       ({ \
      usertype *ret = 0;                                                \
      struct rb_node **new_ = &(root->rb_node), *parent = 0;            \
      while (*new_) {                                                   \
        usertype *this_ = container_of(*new_, usertype, handle);        \
        int result = compare(data->userkey, this_->userkey);            \
        parent = *new_;                                                 \
        if (result < 0)                                                 \
          new_ = &((*new_)->rb_left);                                   \
        else if (result > 0)                                            \
          new_ = &((*new_)->rb_right);                                  \
        else {                                                          \
          ret = this_;                                                  \
          break;                                                        \
        }                                                               \
      }                                                                 \
      if (!ret) {                                                       \
        rb_link_node(&data->handle, parent, new_);                      \
        rb_insert_color(&data->handle, root);                           \
      }                                                                 \
      root->size++;                                                     \
      root->nmodified++;                                                \
      ret; })

#define RB_DELETE(root, usertype, handle, userkey, compare, key)        ({ \
      usertype *p = RB_SEARCH(root, usertype, handle, userkey, compare, key); \
      if (p) {                                                          \
        rb_erase(&p->handle, (root));                                   \
        root->nmodified++;                                              \
        root->size--;                                                   \
      }                                                                 \
      p; })

#define RB_ERASE_LOOP(root)     while (!RB_EMPTY_ROOT(root))
#define RB_ERASE(root, usertype, handle)        ({ \
  usertype *tmp = container_of(root->rb_node, usertype, handle); \
  rb_erase(root->rb_node, root);                                 \
  root->nmodified++;                                             \
  root->size--;                                                  \
  tmp; })

#define RB_ITER(root, usertype, handle, ptr)    \
  for (ptr = rb_first(root); ptr != 0; ptr = rb_next(ptr))


#endif  /* RBTREE_H__ */
