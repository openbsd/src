
/* gmalloc.c - THIS FILE IS AUTOMAGICALLY GENERATED SO DON'T EDIT IT. */

/* Single-file skeleton for GNU malloc.
   Copyright 1989 Free Software Foundation
		  Written May 1989 by Mike Haertel.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

#define __ONEFILE

/* DO NOT DELETE THIS LINE -- ansidecl.h INSERTED HERE. */
/* Copyright (C) 1989 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with the GNU C Library; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* ANSI and traditional C compatibility macros

   ANSI C is assumed if __STDC__ is #defined.

	Macros
		PTR		- Generic pointer type
		LONG_DOUBLE	- `long double' type
		CONST		- `const' keyword
		VOLATILE	- `volatile' keyword
		SIGNED		- `signed' keyword
		PTRCONST	- Generic const pointer (void *const)

	EXFUN(name, prototype)		- declare external function NAME
					  with prototype PROTOTYPE
	DEFUN(name, arglist, args)	- define function NAME with
					  args ARGLIST of types in ARGS
	DEFUN_VOID(name)		- define function NAME with no args
	AND				- argument separator for ARGS
	NOARGS				- null arglist
	DOTS				- `...' in args

    For example:
	extern int EXFUN(printf, (CONST char *format DOTS));
	int DEFUN(fprintf, (stream, format),
		  FILE *stream AND CONST char *format DOTS) { ... }
	void DEFUN_VOID(abort) { ... }
*/

#ifndef	_ANSIDECL_H

#define	_ANSIDECL_H	1


/* Every source file includes this file,
   so they will all get the switch for lint.  */
/* LINTLIBRARY */


#ifdef	__STDC__

#define	PTR		void *
#define	PTRCONST	void *CONST
#define	LONG_DOUBLE	long double

#define	AND		,
#define	NOARGS		void
#define	CONST		const
#define	VOLATILE	volatile
#define	SIGNED		signed
#define	DOTS		, ...

#define	EXFUN(name, proto)		name proto
#define	DEFUN(name, arglist, args)	name(args)
#define	DEFUN_VOID(name)		name(NOARGS)

#else	/* Not ANSI C.  */

#define	PTR		char *
#define	PTRCONST	PTR
#define	LONG_DOUBLE	double

#define	AND		;
#define	NOARGS
#define	CONST
#define	VOLATILE
#define	SIGNED
#define	DOTS

#define	EXFUN(name, proto)		name()
#define	DEFUN(name, arglist, args)	name arglist args;
#define	DEFUN_VOID(name)		name()

#endif	/* ANSI C.  */


#endif	/* ansidecl.h	*/

#ifdef __STDC__
#include <limits.h>
#else
/* DO NOT DELETE THIS LINE -- limits.h INSERTED HERE. */
/* Number of bits in a `char'.  */
#define CHAR_BIT 8

/* No multibyte characters supported yet.  */
#define MB_LEN_MAX 1

/* Minimum and maximum values a `signed char' can hold.  */
#define SCHAR_MIN -128
#define SCHAR_MAX 127

/* Maximum value an `unsigned char' can hold.  (Minimum is 0).  */
#define UCHAR_MAX 255U

/* Minimum and maximum values a `char' can hold.  */
#ifdef __CHAR_UNSIGNED__
#define CHAR_MIN 0
#define CHAR_MAX 255U
#else
#define CHAR_MIN -128
#define CHAR_MAX 127
#endif

/* Minimum and maximum values a `signed short int' can hold.  */
#define SHRT_MIN -32768
#define SHRT_MAX 32767

/* Maximum value an `unsigned short int' can hold.  (Minimum is 0).  */
#define USHRT_MAX 65535U

/* Minimum and maximum values a `signed int' can hold.  */
#define INT_MIN -2147483648
#define INT_MAX 2147483647

/* Maximum value an `unsigned int' can hold.  (Minimum is 0).  */
#define UINT_MAX 4294967295U

/* Minimum and maximum values a `signed long int' can hold.
   (Same as `int').  */
#define LONG_MIN (-LONG_MAX-1)
#define LONG_MAX 2147483647

/* Maximum value an `unsigned long int' can hold.  (Minimum is 0).  */
#define ULONG_MAX 4294967295U
#endif

#ifdef __STDC__
#include <stddef.h>
#else
/* DO NOT DELETE THIS LINE -- stddef.h INSERTED HERE. */
#ifndef _STDDEF_H
#define _STDDEF_H

/* Signed type of difference of two pointers.  */

typedef long ptrdiff_t;

/* Unsigned type of `sizeof' something.  */

#ifndef _SIZE_T	/* in case <sys/types.h> has defined it. */
#define _SIZE_T
typedef unsigned long size_t;
#endif /* _SIZE_T */

/* A null pointer constant.  */

#undef NULL		/* in case <stdio.h> has defined it. */
#define NULL 0

/* Offset of member MEMBER in a struct of type TYPE.  */

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#endif /* _STDDEF_H */
#endif

/* DO NOT DELETE THIS LINE -- stdlib.h INSERTED HERE. */
/* Fake stdlib.h supplying the stuff needed by malloc. */

#ifndef __ONEFILE
#include <stddef.h>
#endif

extern void EXFUN(abort, (void));
extern void EXFUN(free, (PTR));
extern PTR EXFUN(malloc, (size_t));
extern PTR EXFUN(realloc, (PTR, size_t));

/* DO NOT DELETE THIS LINE -- string.h INSERTED HERE. */
/* Fake string.h supplying stuff used by malloc. */
#ifndef __ONEFILE
#include <stddef.h>
#endif

extern PTR EXFUN(memcpy, (PTR, PTR, size_t));
extern PTR EXFUN(memset, (PTR, int, size_t));
#define memmove memcpy

#define _MALLOC_INTERNAL
/* DO NOT DELETE THIS LINE -- malloc.h INSERTED HERE. */
/* Declarations for `malloc' and friends.
   Copyright 1990 Free Software Foundation
		  Written May 1989 by Mike Haertel.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   The author may be reached (Email) at the address mike@@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

#ifndef _MALLOC_H

#define _MALLOC_H	1

#ifndef __ONEFILE
#define	__need_NULL
#define	__need_size_t
#define __need_ptrdiff_t
#include <stddef.h>
#endif

#ifdef _MALLOC_INTERNAL

#ifndef __ONEFILE
#include <limits.h>
#endif

/* The allocator divides the heap into blocks of fixed size; large
   requests receive one or more whole blocks, and small requests
   receive a fragment of a block.  Fragment sizes are powers of two,
   and all fragments of a block are the same size.  When all the
   fragments in a block have been freed, the block itself is freed.  */
#define INT_BIT		(CHAR_BIT * sizeof(int))
#define BLOCKLOG	(INT_BIT > 16 ? 12 : 9)
#define BLOCKSIZE	(1 << BLOCKLOG)
#define BLOCKIFY(SIZE)	(((SIZE) + BLOCKSIZE - 1) / BLOCKSIZE)

/* Determine the amount of memory spanned by the initial heap table
   (not an absolute limit).  */
#define HEAP		(INT_BIT > 16 ? 4194304 : 65536)

/* Number of contiguous free blocks allowed to build up at the end of
   memory before they will be returned to the system.  */
#define FINAL_FREE_BLOCKS	8

/* Where to start searching the free list when looking for new memory.
   The two possible values are 0 and _heapindex.  Starting at 0 seems
   to reduce total memory usage, while starting at _heapindex seems to
   run faster.  */
#define MALLOC_SEARCH_START	_heapindex

/* Data structure giving per-block information.  */
typedef union
  {
    /* Heap information for a busy block.  */
    struct
      {
	/* Zero for a large block, or positive giving the
	   logarithm to the base two of the fragment size.  */
	int type;
	union
	  {
	    struct
	      {
		size_t nfree;	/* Free fragments in a fragmented block.  */
		size_t first;	/* First free fragment of the block.  */
	      } frag;
	    /* Size (in blocks) of a large cluster.  */
	    size_t size;
	  } info;
      } busy;
    /* Heap information for a free block (that may be the first of
       a free cluster).  */
    struct
      {
	size_t size;		/* Size (in blocks) of a free cluster.  */
	size_t next;		/* Index of next free cluster.  */
	size_t prev;		/* Index of previous free cluster.  */
      } free;
  } malloc_info;

/* Pointer to first block of the heap.  */
extern char *_heapbase;

/* Table indexed by block number giving per-block information.  */
extern malloc_info *_heapinfo;

/* Address to block number and vice versa.  */
#define BLOCK(A) (((char *) (A) - _heapbase) / BLOCKSIZE + 1)
#define ADDRESS(B) ((PTR) (((B) - 1) * BLOCKSIZE + _heapbase))

/* Current search index for the heap table.  */
extern size_t _heapindex;

/* Limit of valid info table indices.  */
extern size_t _heaplimit;

/* Doubly linked lists of free fragments.  */
struct list
  {
    struct list *next;
    struct list *prev;
  };

/* Free list headers for each fragment size.  */
extern struct list _fraghead[];

/* Instrumentation.  */
extern size_t _chunks_used;
extern size_t _bytes_used;
extern size_t _chunks_free;
extern size_t _bytes_free;

/* Internal version of free() used in morecore(). */
extern void EXFUN(__free, (PTR __ptr));

#endif  /* _MALLOC_INTERNAL.  */

/* Underlying allocation function; successive calls should
   return contiguous pieces of memory.  */
extern PTR EXFUN((*__morecore), (ptrdiff_t __size));

/* Default value of previous.  */
extern PTR EXFUN(__default_morecore, (ptrdiff_t __size));

/* Flag whether malloc has been called.  */
extern int __malloc_initialized;

/* Hooks for debugging versions.  */
extern void EXFUN((*__free_hook), (PTR __ptr));
extern PTR EXFUN((*__malloc_hook), (size_t __size));
extern PTR EXFUN((*__realloc_hook), (PTR __ptr, size_t __size));

/* Activate a standard collection of debugging hooks.  */
extern void EXFUN(mcheck, (void EXFUN((*func), (void))));

/* Statistics available to the user.  */
struct mstats
  {
    size_t bytes_total;		/* Total size of the heap. */
    size_t chunks_used;		/* Chunks allocated by the user. */
    size_t bytes_used;		/* Byte total of user-allocated chunks. */
    size_t chunks_free;		/* Chunks in the free list. */
    size_t bytes_free;		/* Byte total of chunks in the free list. */
  };

/* Pick up the current statistics. */
extern struct mstats EXFUN(mstats, (NOARGS));

#endif /* malloc.h  */

/* DO NOT DELETE THIS LINE -- free.c INSERTED HERE. */
/* Free a block of memory allocated by `malloc'.
   Copyright 1990 Free Software Foundation
		  Written May 1989 by Mike Haertel.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

#ifndef __ONEFILE
#include "ansidecl.h"
#include <stddef.h>
#include <stdlib.h>

#define _MALLOC_INTERNAL
#include "malloc.h"
#endif /* __ONEFILE */

/* Debugging hook for free.  */
void EXFUN((*__free_hook), (PTR __ptr));

/* Return memory to the heap.  Like free() but don't call a __free_hook
   if there is one.  */
void
DEFUN(__free, (ptr), PTR ptr)
{
  int type;
  size_t block, blocks;
  register size_t i;
  struct list *prev, *next;

  block = BLOCK(ptr);

  type = _heapinfo[block].busy.type;
  switch (type)
    {
    case 0:
      /* Get as many statistics as early as we can.  */
      --_chunks_used;
      _bytes_used -= _heapinfo[block].busy.info.size * BLOCKSIZE;
      _bytes_free += _heapinfo[block].busy.info.size * BLOCKSIZE;

      /* Find the free cluster previous to this one in the free list.
	 Start searching at the last block referenced; this may benefit
	 programs with locality of allocation.  */
      i = _heapindex;
      if (i > block)
	while (i > block)
	  i = _heapinfo[i].free.prev;
      else
	{
	  do
	    i = _heapinfo[i].free.next;
	  while (i > 0 && i < block);
	  i = _heapinfo[i].free.prev;
	}

      /* Determine how to link this block into the free list.  */
      if (block == i + _heapinfo[i].free.size)
	{
	  /* Coalesce this block with its predecessor.  */
	  _heapinfo[i].free.size += _heapinfo[block].busy.info.size;
	  block = i;
	}
      else
	{
	  /* Really link this block back into the free list.  */
	  _heapinfo[block].free.size = _heapinfo[block].busy.info.size;
	  _heapinfo[block].free.next = _heapinfo[i].free.next;
	  _heapinfo[block].free.prev = i;
	  _heapinfo[i].free.next = block;
	  _heapinfo[_heapinfo[block].free.next].free.prev = block;
	  ++_chunks_free;
	}

      /* Now that the block is linked in, see if we can coalesce it
	 with its successor (by deleting its successor from the list
	 and adding in its size).  */
      if (block + _heapinfo[block].free.size == _heapinfo[block].free.next)
	{
	  _heapinfo[block].free.size
	    += _heapinfo[_heapinfo[block].free.next].free.size;
	  _heapinfo[block].free.next
	    = _heapinfo[_heapinfo[block].free.next].free.next;
	  _heapinfo[_heapinfo[block].free.next].free.prev = block;
	  --_chunks_free;
	}

      /* Now see if we can return stuff to the system.  */
      blocks = _heapinfo[block].free.size;
      if (blocks >= FINAL_FREE_BLOCKS && block + blocks == _heaplimit
	  && (*__morecore)(0) == ADDRESS(block + blocks))
	{
	  register size_t bytes = blocks * BLOCKSIZE;
	  _heaplimit -= blocks;
	  (*__morecore)(- bytes);
	  _heapinfo[_heapinfo[block].free.prev].free.next
	    = _heapinfo[block].free.next;
	  _heapinfo[_heapinfo[block].free.next].free.prev
	    = _heapinfo[block].free.prev;
	  block = _heapinfo[block].free.prev;
	  --_chunks_free;
	  _bytes_free -= bytes;
	}

      /* Set the next search to begin at this block.  */
      _heapindex = block;
      break;

    default:
      /* Do some of the statistics.  */
      --_chunks_used;
      _bytes_used -= 1 << type;
      ++_chunks_free;
      _bytes_free += 1 << type;

      /* Get the address of the first free fragment in this block.  */
      prev = (struct list *) ((char *) ADDRESS(block) +
			      (_heapinfo[block].busy.info.frag.first << type));

      if (_heapinfo[block].busy.info.frag.nfree == (BLOCKSIZE >> type) - 1)
	{
	  /* If all fragments of this block are free, remove them
	     from the fragment list and free the whole block.  */
	  next = prev;
	  for (i = 1; i < BLOCKSIZE >> type; ++i)
	    next = next->next;
	  prev->prev->next = next;
	  if (next != NULL)
	    next->prev = prev->prev;
	  _heapinfo[block].busy.type = 0;
	  _heapinfo[block].busy.info.size = 1;

	  /* Keep the statistics accurate.  */
	  ++_chunks_used;
	  _bytes_used += BLOCKSIZE;
	  _chunks_free -= BLOCKSIZE >> type;
	  _bytes_free -= BLOCKSIZE;

	  free(ADDRESS(block));
	}
      else if (_heapinfo[block].busy.info.frag.nfree != 0)
	{
	  /* If some fragments of this block are free, link this
	     fragment into the fragment list after the first free
	     fragment of this block. */
	  next = (struct list *) ptr;
	  next->next = prev->next;
	  next->prev = prev;
	  prev->next = next;
	  if (next->next != NULL)
	    next->next->prev = next;
	  ++_heapinfo[block].busy.info.frag.nfree;
	}
      else
	{
	  /* No fragments of this block are free, so link this
	     fragment into the fragment list and announce that
	     it is the first free fragment of this block. */
	  prev = (struct list *) ptr;
	  _heapinfo[block].busy.info.frag.nfree = 1;
	  _heapinfo[block].busy.info.frag.first = (unsigned int)
	    (((char *) ptr - (char *) NULL) % BLOCKSIZE >> type);
	  prev->next = _fraghead[type].next;
	  prev->prev = &_fraghead[type];
	  prev->prev->next = prev;
	  if (prev->next != NULL)
	    prev->next->prev = prev;
	}
      break;
    }
}

/* Return memory to the heap.  */
void
DEFUN(free, (ptr), PTR ptr)
{
  if (ptr == NULL)
    return;

  if (__free_hook != NULL)
    (*__free_hook)(ptr);
  else
    __free (ptr);
}

/* DO NOT DELETE THIS LINE -- malloc.c INSERTED HERE. */
/* Memory allocator `malloc'.
   Copyright 1990 Free Software Foundation
		  Written May 1989 by Mike Haertel.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

#ifndef __ONEFILE
#include "ansidecl.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define _MALLOC_INTERNAL
#include "malloc.h"
#endif /* __ONEFILE */

/* How to really get more memory.  */
PTR EXFUN((*__morecore), (ptrdiff_t __size)) = __default_morecore;

/* Debugging hook for `malloc'.  */
PTR EXFUN((*__malloc_hook), (size_t __size));

/* Pointer to the base of the first block.  */
char *_heapbase;

/* Block information table.  Allocated with align/__free (not malloc/free).  */
malloc_info *_heapinfo;

/* Number of info entries.  */
static size_t heapsize;

/* Search index in the info table.  */
size_t _heapindex;

/* Limit of valid info table indices.  */
size_t _heaplimit;

/* Free lists for each fragment size.  */
struct list _fraghead[BLOCKLOG];

/* Instrumentation.  */
size_t _chunks_used;
size_t _bytes_used;
size_t _chunks_free;
size_t _bytes_free;

/* Are you experienced?  */
int __malloc_initialized;

/* Aligned allocation.  */
static PTR
DEFUN(align, (size), size_t size)
{
  PTR result;
  unsigned int adj;

  result = (*__morecore)(size);
  adj = (unsigned int) ((char *) result - (char *) NULL) % BLOCKSIZE;
  if (adj != 0)
    {
      adj = BLOCKSIZE - adj;
      (void) (*__morecore)(adj);
      result = (char *) result + adj;
    }
  return result;
}

/* Set everything up and remember that we have.  */
static int
DEFUN_VOID(initialize)
{
  heapsize = HEAP / BLOCKSIZE;
  _heapinfo = (malloc_info *) align(heapsize * sizeof(malloc_info));
  if (_heapinfo == NULL)
    return 0;
  memset(_heapinfo, 0, heapsize * sizeof(malloc_info));
  _heapinfo[0].free.size = 0;
  _heapinfo[0].free.next = _heapinfo[0].free.prev = 0;
  _heapindex = 0;
  _heapbase = (char *) _heapinfo;
  __malloc_initialized = 1;
  return 1;
}

/* Get neatly aligned memory, initializing or
   growing the heap info table as necessary. */
static PTR
DEFUN(morecore, (size), size_t size)
{
  PTR result;
  malloc_info *newinfo, *oldinfo;
  size_t newsize;

  result = align(size);
  if (result == NULL)
    return NULL;

  /* Check if we need to grow the info table.  */
  if (BLOCK((char *) result + size) > heapsize)
    {
      newsize = heapsize;
      while (BLOCK((char *) result + size) > newsize)
	newsize *= 2;
      newinfo = (malloc_info *) align(newsize * sizeof(malloc_info));
      if (newinfo == NULL)
	{
	  (*__morecore)(- size);
	  return NULL;
	}
      memset(newinfo, 0, newsize * sizeof(malloc_info));
      memcpy(newinfo, _heapinfo, heapsize * sizeof(malloc_info));
      oldinfo = _heapinfo;
      newinfo[BLOCK(oldinfo)].busy.type = 0;
      newinfo[BLOCK(oldinfo)].busy.info.size
	= BLOCKIFY(heapsize * sizeof(malloc_info));
      _heapinfo = newinfo;
      __free(oldinfo);
      heapsize = newsize;
    }

  _heaplimit = BLOCK((char *) result + size);
  return result;
}

/* Allocate memory from the heap.  */
PTR
DEFUN(malloc, (size), size_t size)
{
  PTR result;
  size_t block, blocks, lastblocks, start;
  register size_t i;
  struct list *next;

  if (size == 0)
    return NULL;

  if (__malloc_hook != NULL)
    return (*__malloc_hook)(size);

  if (!__malloc_initialized)
    if (!initialize())
      return NULL;

  if (size < sizeof(struct list))
    size = sizeof(struct list);

  /* Determine the allocation policy based on the request size.  */
  if (size <= BLOCKSIZE / 2)
    {
      /* Small allocation to receive a fragment of a block.
	 Determine the logarithm to base two of the fragment size. */
      register size_t log = 1;
      --size;
      while ((size /= 2) != 0)
	++log;

      /* Look in the fragment lists for a
	 free fragment of the desired size. */
      next = _fraghead[log].next;
      if (next != NULL)
	{
	  /* There are free fragments of this size.
	     Pop a fragment out of the fragment list and return it.
	     Update the block's nfree and first counters. */
	  result = (PTR) next;
	  next->prev->next = next->next;
	  if (next->next != NULL)
	    next->next->prev = next->prev;
	  block = BLOCK(result);
	  if (--_heapinfo[block].busy.info.frag.nfree != 0)
	    _heapinfo[block].busy.info.frag.first = (unsigned int)
	      (((char *) next->next - (char *) NULL) % BLOCKSIZE) >> log;

	  /* Update the statistics.  */
	  ++_chunks_used;
	  _bytes_used += 1 << log;
	  --_chunks_free;
	  _bytes_free -= 1 << log;
	}
      else
	{
	  /* No free fragments of the desired size, so get a new block
	     and break it into fragments, returning the first.  */
	  result = malloc(BLOCKSIZE);
	  if (result == NULL)
	    return NULL;

	  /* Link all fragments but the first into the free list.  */
	  for (i = 1; i < BLOCKSIZE >> log; ++i)
	    {
	      next = (struct list *) ((char *) result + (i << log));
	      next->next = _fraghead[log].next;
	      next->prev = &_fraghead[log];
	      next->prev->next = next;
	      if (next->next != NULL)
		next->next->prev = next;
	    }

	  /* Initialize the nfree and first counters for this block.  */
	  block = BLOCK(result);
	  _heapinfo[block].busy.type = log;
	  _heapinfo[block].busy.info.frag.nfree = i - 1;
	  _heapinfo[block].busy.info.frag.first = i - 1;

	  _chunks_free += (BLOCKSIZE >> log) - 1;
	  _bytes_free += BLOCKSIZE - (1 << log);
	}
    }
  else
    {
      /* Large allocation to receive one or more blocks.
	 Search the free list in a circle starting at the last place visited.
	 If we loop completely around without finding a large enough
	 space we will have to get more memory from the system.  */
      blocks = BLOCKIFY(size);
      start = block = MALLOC_SEARCH_START;
      while (_heapinfo[block].free.size < blocks)
	{
	  block = _heapinfo[block].free.next;
	  if (block == start)
	    {
	      /* Need to get more from the system.  Check to see if
		 the new core will be contiguous with the final free
		 block; if so we don't need to get as much.  */
	      block = _heapinfo[0].free.prev;
	      lastblocks = _heapinfo[block].free.size;
	      if (_heaplimit != 0 && block + lastblocks == _heaplimit &&
		  (*__morecore)(0) == ADDRESS(block + lastblocks) &&
		  (morecore((blocks - lastblocks) * BLOCKSIZE)) != NULL)
		{
		  _heapinfo[block].free.size = blocks;
		  _bytes_free += (blocks - lastblocks) * BLOCKSIZE;
		  continue;
		}
	      result = morecore(blocks * BLOCKSIZE);
	      if (result == NULL)
		return NULL;
	      block = BLOCK(result);
	      _heapinfo[block].busy.type = 0;
	      _heapinfo[block].busy.info.size = blocks;
	      ++_chunks_used;
	      _bytes_used += blocks * BLOCKSIZE;
	      return result;
	    }
	}

      /* At this point we have found a suitable free list entry.
	 Figure out how to remove what we need from the list. */
      result = ADDRESS(block);
      if (_heapinfo[block].free.size > blocks)
	{
	  /* The block we found has a bit left over,
	     so relink the tail end back into the free list. */
	  _heapinfo[block + blocks].free.size
	    = _heapinfo[block].free.size - blocks;
	  _heapinfo[block + blocks].free.next
	    = _heapinfo[block].free.next;
	  _heapinfo[block + blocks].free.prev
	    = _heapinfo[block].free.prev;
	  _heapinfo[_heapinfo[block].free.prev].free.next
	    = _heapinfo[_heapinfo[block].free.next].free.prev
	      = _heapindex = block + blocks;
	}
      else
	{
	  /* The block exactly matches our requirements,
	     so just remove it from the list. */
	  _heapinfo[_heapinfo[block].free.next].free.prev
	    = _heapinfo[block].free.prev;
	  _heapinfo[_heapinfo[block].free.prev].free.next
	    = _heapindex = _heapinfo[block].free.next;
	  --_chunks_free;
	}

      _heapinfo[block].busy.type = 0;
      _heapinfo[block].busy.info.size = blocks;
      ++_chunks_used;
      _bytes_used += blocks * BLOCKSIZE;
      _bytes_free -= blocks * BLOCKSIZE;
    }

  return result;
}

/* DO NOT DELETE THIS LINE -- realloc.c INSERTED HERE. */
/* Change the size of a block allocated by `malloc'.
   Copyright 1990 Free Software Foundation
		  Written May 1989 by Mike Haertel.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

#ifndef __ONEFILE
#include "ansidecl.h"
#include <stdlib.h>
#include <string.h>

#define _MALLOC_INTERNAL
#include "malloc.h"
#endif /* __ONEFILE */

#define MIN(A, B) ((A) < (B) ? (A) : (B))

/* Debugging hook for realloc.  */
PTR EXFUN((*__realloc_hook), (PTR __ptr, size_t __size));

/* Resize the given region to the new size, returning a pointer
   to the (possibly moved) region.  This is optimized for speed;
   some benchmarks seem to indicate that greater compactness is
   achieved by unconditionally allocating and copying to a
   new region.  This module has incestuous knowledge of the
   internals of both free and malloc. */
PTR
DEFUN(realloc, (ptr, size), PTR ptr AND size_t size)
{
  PTR result;
  int type;
  size_t block, blocks, oldlimit;

  if (size == 0)
    {
      free(ptr);
      return NULL;
    }
  else if (ptr == NULL)
    return malloc(size);

  if (__realloc_hook != NULL)
    return (*__realloc_hook)(ptr, size);

  block = BLOCK(ptr);

  type = _heapinfo[block].busy.type;
  switch (type)
    {
    case 0:
      /* Maybe reallocate a large block to a small fragment.  */
      if (size <= BLOCKSIZE / 2)
	{
	  result = malloc(size);
	  if (result != NULL)
	    {
	      memcpy(result, ptr, size);
	      free(ptr);
	      return result;
	    }
	}

      /* The new size is a large allocation as well;
	 see if we can hold it in place. */
      blocks = BLOCKIFY(size);
      if (blocks < _heapinfo[block].busy.info.size)
	{
	  /* The new size is smaller; return
	     excess memory to the free list. */
	  _heapinfo[block + blocks].busy.type = 0;
	  _heapinfo[block + blocks].busy.info.size
	    = _heapinfo[block].busy.info.size - blocks;
	  _heapinfo[block].busy.info.size = blocks;
	  free(ADDRESS(block + blocks));
	  result = ptr;
	}
      else if (blocks == _heapinfo[block].busy.info.size)
	/* No size change necessary.  */
	result = ptr;
      else
	{
	  /* Won't fit, so allocate a new region that will.
	     Free the old region first in case there is sufficient
	     adjacent free space to grow without moving. */
	  blocks = _heapinfo[block].busy.info.size;
	  /* Prevent free from actually returning memory to the system.  */
	  oldlimit = _heaplimit;
	  _heaplimit = 0;
	  free(ptr);
	  _heaplimit = oldlimit;
	  result = malloc(size);
	  if (result == NULL)
	    {
	      (void) malloc(blocks * BLOCKSIZE);
	      return NULL;
	    }
	  if (ptr != result)
	    memmove(result, ptr, blocks * BLOCKSIZE);
	}
      break;

    default:
      /* Old size is a fragment; type is logarithm
	 to base two of the fragment size.  */
      if (size > 1 << (type - 1) && size <= 1 << type)
	/* The new size is the same kind of fragment.  */
	result = ptr;
      else
	{
	  /* The new size is different; allocate a new space,
	     and copy the lesser of the new size and the old. */
	  result = malloc(size);
	  if (result == NULL)
	    return NULL;
	  memcpy(result, ptr, MIN(size, 1 << type));
	  free(ptr);
	}
      break;
    }

  return result;
}

/* DO NOT DELETE THIS LINE -- unix.c INSERTED HERE. */
/* unix.c - get more memory with a UNIX system call.
   Copyright 1990 Free Software Foundation
		  Written May 1989 by Mike Haertel.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

#ifndef __ONEFILE
#include "ansidecl.h"
#include <stddef.h>

#define _MALLOC_INTERNAL
#include "malloc.h"
#endif /* __ONEFILE */

extern PTR EXFUN(sbrk, (ptrdiff_t size));

PTR
DEFUN(__default_morecore, (size), ptrdiff_t size)
{
  PTR result;

  result = sbrk(size);
  if (result == (PTR) -1)
    return NULL;
  return result;
}

#define __getpagesize getpagesize
/* DO NOT DELETE THIS LINE -- valloc.c INSERTED HERE. */
/* Allocate memory on a page boundary.
   Copyright 1990 Free Software Foundation
		  Written May 1989 by Mike Haertel.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

#ifndef __ONEFILE
#include "ansidecl.h"
#include <stdlib.h>
#endif /* __ONEFILE */

extern size_t EXFUN(__getpagesize, (NOARGS));

static size_t pagesize;

PTR
DEFUN(valloc, (size), size_t size)
{
  PTR result;
  unsigned int adj;

  if (pagesize == 0)
    pagesize = __getpagesize();

  result = malloc(size + pagesize);
  if (result == NULL)
    return NULL;
  adj = (unsigned int) ((char *) result - (char *) NULL) % pagesize;
  if (adj != 0)
    result = (char *) result + pagesize - adj;
  return result;
}
