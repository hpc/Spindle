/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU Lesser General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

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
#include <stdlib.h>

#define SHEEP_NULL ((void *) sheep_base)
#define IS_SHEEP_NULL(S) ((S)->val == 0)

typedef struct {
   uint32_t val;
} sheep_ptr_t;

extern unsigned char *sheep_base;
static int  sheep_ptr_equals(sheep_ptr_t a, sheep_ptr_t b) { return a.val == b.val; }
static void *sheep_ptr(sheep_ptr_t *p) { return (p->val == 0) ? NULL : (void *) ((((uint64_t) p->val) << 3) + sheep_base); }
static sheep_ptr_t ptr_sheep(void *v) { sheep_ptr_t p; p.val = (v == NULL) ? 0 : (((uint64_t) v) - ((uint64_t) sheep_base)) >> 3; return p; }
static void set_sheep_ptr(sheep_ptr_t *p, void *v) { *p = ptr_sheep(v); }

extern void init_sheep(void *mem, size_t size, int use_first_fit);
extern void *malloc_sheep(size_t size);
extern void free_sheep(void *p);
extern size_t sheep_alloc_size(size_t size);
extern void validate_sheep();

#ifdef __GNUC__
#define UNUSED_ATTR __attribute__((unused))
#else
#define UNUSED_ATTR
#endif
static void unused_sheep_vars() UNUSED_ATTR;
static void unused_sheep_vars() { (void) sheep_ptr; (void) ptr_sheep; (void) set_sheep_ptr; (void) sheep_ptr_equals; (void) volatile_sheep_ptr; }

#endif
