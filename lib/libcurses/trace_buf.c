/*	$OpenBSD: trace_buf.c,v 1.1 1997/12/03 05:21:44 millert Exp $	*/

/******************************************************************************
 * Copyright 1997 by Thomas E. Dickey <dickey@clark.net>                      *
 * All Rights Reserved.                                                       *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software and its      *
 * documentation for any purpose and without fee is hereby granted, provided  *
 * that the above copyright notice appear in all copies and that both that    *
 * copyright notice and this permission notice appear in supporting           *
 * documentation, and that the name of the above listed copyright holder(s)   *
 * not be used in advertising or publicity pertaining to distribution of the  *
 * software without specific, written prior permission.                       *
 *                                                                            *
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD   *
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND  *
 * FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE  *
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES          *
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN      *
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR *
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.                *
 ******************************************************************************/
/*
 *	trace_buf.c - Tracing/Debugging buffers (attributes)
 */

#include <curses.priv.h>

MODULE_ID("Id: trace_buf.c,v 1.2 1997/10/26 22:09:05 tom Exp $")

char * _nc_trace_buf(int bufnum, size_t want)
{
	static struct {
		char *text;
		size_t size;
	} *list;
	static size_t have;

#if NO_LEAKS
	if (bufnum < 0) {
		if (have) {
			while (have--) {
				free(list[have].text);
			}
			free(list);
		}
		return 0;
	}
#endif

	if ((size_t)(bufnum+1) > have) {
		size_t need = (bufnum + 1) * 2;
		size_t used = sizeof(*list) * need;
		list = (list == 0) ? malloc(used) : realloc(list, used);
		if (list == 0) {
			errno = ENOMEM;
			return(NULL);
		}
		while (need > have)
			list[have++].text = 0;
	}

	if (list[bufnum].text == 0)
	{
		list[bufnum].text = malloc(want);
		list[bufnum].size = want;
	}
	else if (want > list[bufnum].size) {
		list[bufnum].text = realloc(list[bufnum].text, want);
		list[bufnum].size = want;
	}
	if (list[bufnum].text != 0)
		*(list[bufnum].text) = '\0';
	else
		errno = ENOMEM;
	return list[bufnum].text;
}
