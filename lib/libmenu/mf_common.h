/*-----------------------------------------------------------------------------+
|           The ncurses menu library is  Copyright (C) 1995-1997               |
|             by Juergen Pfeifer <Juergen.Pfeifer@T-Online.de>                 |
|                          All Rights Reserved.                                |
|                                                                              |
| Permission to use, copy, modify, and distribute this software and its        |
| documentation for any purpose and without fee is hereby granted, provided    |
| that the above copyright notice appear in all copies and that both that      |
| copyright notice and this permission notice appear in supporting             |
| documentation, and that the name of the above listed copyright holder(s) not |
| be used in advertising or publicity pertaining to distribution of the        |
| software without specific, written prior permission.                         | 
|                                                                              |
| THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD TO  |
| THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FIT-  |
| NESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR   |
| ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RE- |
| SULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, |
| NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH    |
| THE USE OR PERFORMANCE OF THIS SOFTWARE.                                     |
+-----------------------------------------------------------------------------*/

/* Common internal header for menu and form library */

#if HAVE_CONFIG_H
#  include <ncurses_cfg.h>
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#if !HAVE_EXTERN_ERRNO
extern int errno;
#endif

#if HAVE_EXTERN_ERRNO
#include <errno.h>
#endif

/* in case of debug version we ignore the suppression of assertions */
#ifdef TRACE
#  ifdef NDEBUG
#    undef NDEBUG
#  endif
#endif

#include <nc_alloc.h>

#ifdef USE_RCS_IDS
#define MODULE_ID(id) static const char Ident[] = id;
#else
#define MODULE_ID(id) /*nothing*/
#endif


/* Maximum regular 8-bit character code */
#define MAX_REGULAR_CHARACTER (0xff)

#define SET_ERROR(code) (errno=(code))
#define RETURN(code) return( SET_ERROR(code) )

/* The few common values in the status fields for menus and forms */
#define _POSTED         (0x01)  /* menu or form is posted                  */
#define _IN_DRIVER      (0x02)  /* menu or form is processing hook routine */

/* Call object hook */
#define Call_Hook( object, handler ) \
   if ( (object) && ((object)->handler) )\
   {\
	(object)->status |= _IN_DRIVER;\
	(object)->handler(object);\
	(object)->status &= ~_IN_DRIVER;\
   }

#define INLINE

#ifndef TRACE
#  if CC_HAS_INLINE_FUNCS
#    undef INLINE
#    define INLINE inline
#  endif
#endif
