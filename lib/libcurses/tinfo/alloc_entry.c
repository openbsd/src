/*	$OpenBSD: alloc_entry.c,v 1.1 1999/01/18 19:10:13 millert Exp $	*/

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
 * alloc_entry.c -- allocation functions for terminfo entries
 *
 *	_nc_init_entry()
 *	_nc_save_str()
 *	_nc_merge_entry();
 *	_nc_wrap_entry();
 *
 */

#include <curses.priv.h>

#include <tic.h>
#include <term.h>
#include <term_entry.h>

MODULE_ID("$From: alloc_entry.c,v 1.14 1998/07/04 23:17:42 tom Exp $")

#define ABSENT_OFFSET    -1
#define CANCELLED_OFFSET -2

#define MAX_STRTAB	4096	/* documented maximum entry size */

static char	stringbuf[MAX_STRTAB];	/* buffer for string capabilities */
static size_t	next_free;		/* next free character in stringbuf */

void _nc_init_entry(TERMTYPE *const tp)
/* initialize a terminal type data block */
{
int	i;

	for (i=0; i < BOOLCOUNT; i++)
		tp->Booleans[i] = FALSE; /* FIXME: why not ABSENT_BOOLEAN? */

	for (i=0; i < NUMCOUNT; i++)
		tp->Numbers[i] = ABSENT_NUMERIC;

	for (i=0; i < STRCOUNT; i++)
		tp->Strings[i] = ABSENT_STRING;

	next_free = 0;
}

char *_nc_save_str(const char *const string)
/* save a copy of string in the string buffer */
{
size_t	old_next_free = next_free;
size_t	len = strlen(string) + 1;

	if (next_free + len < MAX_STRTAB)
	{
		strcpy(&stringbuf[next_free], string);
		DEBUG(7, ("Saved string %s", _nc_visbuf(string)));
		DEBUG(7, ("at location %d", (int) next_free));
		next_free += len;
	}
	return(stringbuf + old_next_free);
}

void _nc_wrap_entry(ENTRY *const ep)
/* copy the string parts to allocated storage, preserving pointers to it */
{
int	offsets[STRCOUNT], useoffsets[MAX_USES];
int	i, n;

	n = ep->tterm.term_names - stringbuf;
	for (i=0; i < STRCOUNT; i++)
		if (ep->tterm.Strings[i] == ABSENT_STRING)
			offsets[i] = ABSENT_OFFSET;
		else if (ep->tterm.Strings[i] == CANCELLED_STRING)
			offsets[i] = CANCELLED_OFFSET;
		else
			offsets[i] = ep->tterm.Strings[i] - stringbuf;

	for (i=0; i < ep->nuses; i++)
		if (ep->uses[i].parent == (void *)0)
			useoffsets[i] = ABSENT_OFFSET;
		else
			useoffsets[i] = (char *)(ep->uses[i].parent) - stringbuf;

	if ((ep->tterm.str_table = (char *)malloc(next_free)) == (char *)0)
		_nc_err_abort("Out of memory");
	(void) memcpy(ep->tterm.str_table, stringbuf, next_free);

	ep->tterm.term_names = ep->tterm.str_table + n;
	for (i=0; i < STRCOUNT; i++)
		if (offsets[i] == ABSENT_OFFSET)
			ep->tterm.Strings[i] = ABSENT_STRING;
		else if (offsets[i] == CANCELLED_OFFSET)
			ep->tterm.Strings[i] = CANCELLED_STRING;
		else
			ep->tterm.Strings[i] = ep->tterm.str_table + offsets[i];

	for (i=0; i < ep->nuses; i++)
		if (useoffsets[i] == ABSENT_OFFSET)
			ep->uses[i].parent = (void *)0;
		else
			ep->uses[i].parent = (char *)(ep->tterm.str_table + useoffsets[i]);
}

void _nc_merge_entry(TERMTYPE *const to, TERMTYPE *const from)
/* merge capabilities from `from' entry into `to' entry */
{
    int	i;

    for (i=0; i < BOOLCOUNT; i++)
    {
	int	mergebool = from->Booleans[i];

	if (mergebool == CANCELLED_BOOLEAN)
	    to->Booleans[i] = FALSE;
	else if (mergebool == TRUE)
	    to->Booleans[i] = mergebool;
    }

    for (i=0; i < NUMCOUNT; i++)
    {
	int	mergenum = from->Numbers[i];

	if (mergenum == CANCELLED_NUMERIC)
	    to->Numbers[i] = ABSENT_NUMERIC;
	else if (mergenum != ABSENT_NUMERIC)
	    to->Numbers[i] = mergenum;
    }

    /*
     * Note: the copies of strings this makes don't have their own
     * storage.  This is OK right now, but will be a problem if we
     * we ever want to deallocate entries.
     */
    for (i=0; i < STRCOUNT; i++)
    {
	char	*mergestring = from->Strings[i];

	if (mergestring == CANCELLED_STRING)
	    to->Strings[i] = ABSENT_STRING;
	else if (mergestring != ABSENT_STRING)
	    to->Strings[i] = mergestring;
    }
}

