/******************************************************************************
 * Copyright 1996,1997 by Thomas E. Dickey <dickey@clark.net>                 *
 * All Rights Reserved.                                                       *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software and its      *
 * documentation for any purpose and without fee is hereby granted, provided  *
 * that the above copyright notice appear in all copies and that both that    *
 * copyright notice and this permission notice appear in supporting           *
 * documentation, and that the name of the above listed copyright holder(s)   *
 * not be used in advertising or publicity pertaining to distribution of the  *
 * software without specific, written prior permission. THE ABOVE LISTED      *
 * COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,  *
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO     *
 * EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY         *
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER       *
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF       *
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN        *
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.                   *
 ******************************************************************************/
/* Id: nc_alloc.h,v 1.4 1997/02/15 18:51:39 tom Exp $ */

#ifndef NC_ALLOC_included
#define NC_ALLOC_included 1

#if HAVE_LIBDMALLOC
#include <dmalloc.h>    /* Gray Watson's library */
#else
#undef  HAVE_LIBDMALLOC
#define HAVE_LIBDMALLOC 0
#endif

#if HAVE_LIBDBMALLOC
#include <dbmalloc.h>   /* Conor Cahill's library */
#else
#undef  HAVE_LIBDBMALLOC
#define HAVE_LIBDBMALLOC 0
#endif

#ifndef NO_LEAKS
#define NO_LEAKS 0
#endif

#ifndef HAVE_NC_FREEALL
#define HAVE_NC_FREEALL 0
#endif

#if HAVE_LIBDBMALLOC || HAVE_LIBDMALLOC || NO_LEAKS || HAVE_NC_FREEALL
struct termtype;
extern void _nc_free_and_exit(int) GCC_NORETURN;
extern void _nc_free_tparm(void);
extern void _nc_leaks_dump_entry(void);
extern void _nc_free_termtype(struct termtype *, int);
#define ExitProgram(code) _nc_free_and_exit(code)
#endif

#ifndef ExitProgram
#define ExitProgram(code) return code
#endif

#endif /* NC_ALLOC_included */
