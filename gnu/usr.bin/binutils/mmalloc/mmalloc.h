#ifndef MMALLOC_H
#define MMALLOC_H 1

/*  FIXME:  If <stddef.h> doesn't exist, you'll need to do something
            to define size_t before including this file.  Like upgrading
            to a system with an ANSI C environment. */

#ifdef HAVE_STDDEF_H
#  include <stddef.h>
#endif

#include "ansidecl.h"
 
/* Allocate SIZE bytes of memory.  */

extern PTR mmalloc PARAMS ((PTR, size_t));

/* Re-allocate the previously allocated block in PTR, making the new block
   SIZE bytes long.  */

extern PTR mrealloc PARAMS ((PTR, PTR, size_t));

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */

extern PTR mcalloc PARAMS ((PTR, size_t, size_t));

/* Free a block allocated by `mmalloc', `mrealloc' or `mcalloc'.  */

extern void mfree PARAMS ((PTR, PTR));

/* Allocate SIZE bytes allocated to ALIGNMENT bytes.  */

extern PTR mmemalign PARAMS ((PTR, size_t, size_t));

/* Allocate SIZE bytes on a page boundary.  */

extern PTR mvalloc PARAMS ((PTR, size_t));

/* Activate a standard collection of debugging hooks.  */

extern int mmcheck PARAMS ((PTR, void (*) (void)));

extern int mmcheckf PARAMS ((PTR, void (*) (void), int));

/* Pick up the current statistics. (see FIXME elsewhere) */

extern struct mstats mmstats PARAMS ((PTR));

extern PTR mmalloc_attach PARAMS ((int, PTR));

extern PTR mmalloc_detach PARAMS ((PTR));

extern int mmalloc_setkey PARAMS ((PTR, int, PTR));

extern PTR mmalloc_getkey PARAMS ((PTR, int));

extern int mmalloc_errno PARAMS ((PTR));

extern int mmtrace PARAMS ((void));

extern PTR mmalloc_findbase PARAMS ((int));

#endif  /* MMALLOC_H */
