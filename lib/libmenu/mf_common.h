
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

/* Common internal header for menu and form library */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
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

#define INLINE __inline
