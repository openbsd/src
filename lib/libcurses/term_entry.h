/*	$OpenBSD: term_entry.h,v 1.4 1999/01/23 18:31:02 millert Exp $	*/

/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/


/*
 *	term_entry.h -- interface to entry-manipulation code
 */

#ifndef _TERM_ENTRY_H
#define _TERM_ENTRY_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_USES	32

typedef struct entry {
	TERMTYPE	tterm;
	int		nuses;
	struct
        {
	    void	*parent;	/* (char *) or (ENTRY *) */
	    long	line;
        }
	uses[MAX_USES];
	long		cstart, cend;
	long		startline;
	struct entry	*next;
	struct entry	*last;
}
ENTRY;

extern ENTRY	*_nc_head, *_nc_tail;
#define for_entry_list(qp)	for (qp = _nc_head; qp; qp = qp->next)

#define MAX_LINE	132

#define NULLHOOK        (bool(*)(ENTRY *))0

/* alloc_entry.c: elementary allocation code */
extern void _nc_init_entry(TERMTYPE *const);
extern char *_nc_save_str(const char *const);
extern void _nc_merge_entry(TERMTYPE *const, TERMTYPE *const);
extern void _nc_wrap_entry(ENTRY *const);

/* parse_entry.c: entry-parsing code */
extern int _nc_parse_entry(ENTRY *, int, bool);
extern int _nc_capcmp(const char *, const char *);

/* write_entry.c: writing an entry to the file system */
extern void _nc_set_writedir(char *);
extern void _nc_write_entry(TERMTYPE *const);

/* comp_parse.c: entry list handling */
extern void _nc_read_entry_source(FILE*, char*, int, bool, bool (*)(ENTRY*));
extern bool _nc_entry_match(char *, char *);
extern int _nc_resolve_uses(void);
extern void _nc_free_entries(ENTRY *);

/* read_bsd_terminfo.c: terminfo.db reading */
extern int _nc_read_bsd_terminfo_entry(const char * const, char * const, TERMTYPE *const);
extern int _nc_read_bsd_terminfo_file(const char * const, TERMTYPE *const);

#ifdef __cplusplus
}
#endif

#endif /* _TERM_ENTRY_H */
