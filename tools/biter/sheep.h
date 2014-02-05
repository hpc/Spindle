#if !defined(SHEEP_H_)
#define SHEEP_H_
/**
 * sheep stands for "shared heap".  It's a malloc/free system that's safe to use
 * in memory shared between processes.  It's up to higher-level code to make to
 * serialize access to the heap.  sheep's main responsibility is making sure that
 * the internal pointers in the heap are position independent (and thus the heap
 * can be mapped at different locations in different processes).
 *
 * It's also up to the user to ensure that any pointers they store in the shared heap:
 *  - only point at other things in the shared heap
 *  - are also position-independent pointers.
 *
 * To help with the position-independent pointers use the sheep_ptr_t for any pointer
 * stored in the shared heap.  Store a regular pointer into a sheep_ptr_t using set_sheep_ptr(),
 * and retrieve a regular pointer back out using sheep_ptr().
 **/

#include <stdint.h>

#define SHEEP_NULL ((void *) sheep_base)
#define IS_SHEEP_NULL(S) ((S)->val == 0)

typedef struct {
   uint32_t val;
} sheep_ptr_t;

extern unsigned char *sheep_base;
static void *sheep_ptr(sheep_ptr_t *p) { return (void *) ((((uint64_t) p->val) << 3) + sheep_base); }
static sheep_ptr_t ptr_sheep(void *v) { sheep_ptr_t p; p.val = (((uint64_t) v) - ((uint64_t) sheep_base)) >> 3; return p; }
static void set_sheep_ptr(sheep_ptr_t *p, void *v) { *p = ptr_sheep(v); }

extern void init_sheep(void *mem, size_t size, int use_first_fit);
extern void *malloc_sheep(size_t size);
extern void free_sheep(void *p);

#ifdef __GNUC__
#define UNUSED_ATTR __attribute__((unused))
#else
#define UNUSED_ATTR
#endif
static void unused_sheep_vars() UNUSED_ATTR;
static void unused_sheep_vars() { (void) sheep_ptr; (void) ptr_sheep; (void) set_sheep_ptr; }


#endif
