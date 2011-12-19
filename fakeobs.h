/*
 * GNU obstack emulation, useful for debugging.
 * Copyright (C) 2004  Seong-Kook Shin <cinsky@gmail.com>
 */
#ifndef FAKEOBS_H_
#define FAKEOBS_H_


/* This indirect using of extern "C" { ... } makes Emacs happy */
#ifndef BEGIN_C_DECLS
# ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
# else
#  define BEGIN_C_DECLS
#  define END_C_DECLS
# endif
#endif /* BEGIN_C_DECLS */

BEGIN_C_DECLS

/*
 * This module is only for debugging. You should not use this
 * in release version of your program. The best usage is to make a symbolic
 * link of this module as obstack.[ch]:
 *
 * $ ln -s fakeobs.h obstack.h
 * $ ln -s fakeobs.c obstack.c
 *
 * If your obstack-related code is suspicious, link your program with
 * -lefence with this module.
 *
 * Description of the rest functions and macros are found in
 * GNU obstack manual.
 */
struct obstack {
  int grow;
  int grow_size;

  int num_ptrs;
  int current;
  void **ptrs;
};


/* You should define fake_obstack_malloc, fake_obstack_free, and
 * fake_obstack_realloc appropriately.  obstack_chunk_alloc and
 * obstack_chunk_free are not used in this module. */
#ifndef fake_obstack_alloc
#define fake_obstack_alloc      malloc
#define fake_obstack_free       free
#define fake_obstack_realloc    realloc
#endif /* 0 */


extern void (*obstack_alloc_failed_handler)(void);

#define obstack_init(s)         obstack_init_((s), (fake_obstack_alloc))
#define obstack_free(s, p)      obstack_free_((s), (p), (fake_obstack_free))
#define obstack_alloc(s, size)  obstack_alloc_((s), (size), \
                                               (fake_obstack_realloc))
#define obstack_copy(s, a, z)   obstack_copy_((s), (a), (z), \
                                              (fake_obstack_realloc))
#define obstack_copy0(s, a, z)  obstack_copy0_((s), (a), (z), \
                                               (fake_obstack_realloc))
#define obstack_blank(s, z)     obstack_blank_((s), (z), \
                                               (fake_obstack_realloc))
#define obstack_grow(s, a, z)   obstack_grow_((s), (a), (z), \
                                              (fake_obstack_realloc))
#define obstack_grow0(s, a, z)  obstack_grow0_((s), (a), (z), \
                                               (fake_obstack_realloc))
#define obstack_1grow(s, d)     obstack_1grow_((s), (d), \
                                               (fake_obstack_realloc))

extern void obstack_init_(struct obstack *stack, void *(*alloc_func)(size_t));
extern void obstack_free_(struct obstack *stack, void *ptr,
                          void (*free_func)(void *));

extern void *obstack_alloc_(struct obstack *stack, int size,
                            void *(*realloc_func)(void *, size_t));
extern void *obstack_copy_(struct obstack *stack,
                           const void *address, int size,
                           void *(*realloc_func)(void *, size_t));
extern void *obstack_copy0_(struct obstack *stack,
                            const void *address, int size,
                            void *(*realloc_func)(void *, size_t));
extern void obstack_blank_(struct obstack *stack, int size,
                           void *(*realloc_func)(void *, size_t));
extern void obstack_grow_(struct obstack *stack,
                          const void *address, int size,
                          void *(*realloc_func)(void *, size_t));
extern void obstack_grow0_(struct obstack *stack,
                           const void *address, int size,
                           void *(*realloc_func)(void *, size_t));
extern void obstack_1grow_(struct obstack *stack, char data,
                           void *(*realloc_func)(void *, size_t));

extern void obstack_ptr_grow(struct obstack *s, void *data);
extern void obstack_int_grow(struct obstack *s, int data);

#define obstack_begin(s, z)     obstack_init((s))


static __inline__ void *
obstack_finish(struct obstack *stack)
{
  stack->grow = 0;
  stack->grow_size = 0;
  return stack->ptrs[stack->current];
}


static __inline__ int
obstack_object_size(struct obstack *stack)
{
  return stack->grow_size;
}


static __inline__ void *
obstack_object_base(struct obstack *stack)
{
  return stack->ptrs[stack->current];
}


static __inline__ void *
obstack_base(struct obstack *stack)
{
  return stack->ptrs[stack->current];
}


static __inline__ void *
obstack_next_free(struct obstack *stack)
{
  return obstack_base(stack);
}


static __inline__ int
obstack_room(struct obstack *stack)
{
  return 0;
}


extern void obstack_ptr_grow_fast(struct obstack *stack, void *data);
extern void obstack_int_grow_fast(struct obstack *stack, int data);
extern void obstack_blank_fast(struct obstack *stack, int size);
extern void obstack_1grow_fast(struct obstack *stack, char c);

extern int fake_alignment_mask;


#define obstack_alignment_mask(stack)   (fake_alignment_mask)
#define obstack_chunk_size(stack)       (1)

END_C_DECLS

#endif /* FAKEOBS_H_ */
