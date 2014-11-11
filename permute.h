#ifndef PERMUTE_H__
#define PERMUTE_H__

/* This indirect using of extern "C" { ... } makes Emacs happy */
#ifndef BEGIN_C_DECLS
# ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   };
# else
#  define BEGIN_C_DECLS
#  define END_C_DECLS
# endif
#endif /* BEGIN_C_DECLS */

#include <stddef.h>

BEGIN_C_DECLS

/*
 * int (*cb)(const int *elems, size_t nelem, void *data);
 *
 * All three functions will call the callback, CB with the current
 * selected sequence, ELEMS with the length, NELEM.
 *
 * If CB returns nonzero, all three functions will immediately return
 * to the caller, with the total number of sequence it found so far.
 */

/*
 * Return the number of permutation of the set, { 0, ..., N - 1 }.
 *
 * This function will call CB every sequence it found.  If CB
 * returns nonzero, it will immediately return to the caller with the
 * total number of sequences it found so far.
 */
extern int permute(size_t n, int (*cb)(const int *, size_t, void *),
                   void *data);

/*
 * Return the number of K element(s) subset of the set, { 0, ..., N - 1 }.
 *
 * This function will call CB every sequence it found.  If CB
 * returns nonzero, it will immediately return to the caller with the
 * total number of sequences it found so far.
 */
extern int ksubset(int n, int k, int (*cb)(const int *, size_t, void *),
                   void *data);

/*
 * Return the number of all subset of the set, { 0, ..., N - 1 }.
 *
 * This function will call CB every sequence it found.  If CB
 * returns nonzero, it will immediately return to the caller with the
 * total number of sequences it found so far.
 */
extern int subset(int n, int (*cb)(const int *, size_t, void *),
                  void *data);

END_C_DECLS

#endif /* PERMUTE_H__ */
