/*	$OpenBSD: tries.c,v 1.1 1997/12/03 05:21:45 millert Exp $	*/

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
 * software without specific, written prior permission. THE ABOVE LISTED      *
 * COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,  *
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO     *
 * EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY         *
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER       *
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF       *
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN        *
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.                   *
 ******************************************************************************/

/*
**	tries.c
**
**	Functions to manage the tree of partial-completions for keycodes.
**
*/

#include <curses.priv.h>

MODULE_ID("Id: tries.c,v 1.5 1997/10/26 21:55:15 tom Exp $")

#define SET_TRY(dst,src) if ((dst->ch = *src++) == 128) dst->ch = '\0'

void _nc_add_to_try(struct tries **tree, char *str, unsigned short code)
{
	static bool     out_of_memory = FALSE;
	struct tries    *ptr, *savedptr;
	unsigned char	*txt = (unsigned char *)str;

	if (txt == 0 || out_of_memory || code == 0)
		return;

	if ((*tree) != 0) {
		ptr = savedptr = (*tree);

		for (;;) {
			unsigned char cmp = *txt;

			while (ptr->ch != cmp
			       &&  ptr->sibling != 0)
				ptr = ptr->sibling;
	
			if (ptr->ch == cmp) {
				if (*(++txt) == '\0') {
					ptr->value = code;
					return;
				}
				if (ptr->child != 0)
					ptr = ptr->child;
				else
					break;
			} else {
				if ((ptr->sibling = typeCalloc(struct tries,1)) == 0) {
					out_of_memory = TRUE;
					return;
				}

				savedptr = ptr = ptr->sibling;
				SET_TRY(ptr,txt);
				ptr->value = 0;

				break;
			}
		} /* end for (;;) */
	} else {   /* (*tree) == 0 :: First sequence to be added */
		savedptr = ptr = (*tree) = typeCalloc(struct tries,1);

		if (ptr == 0) {
			out_of_memory = TRUE;
			return;
		}

		SET_TRY(ptr,txt);
		ptr->value = 0;
	}

	    /* at this point, we are adding to the try.  ptr->child == 0 */

	while (*txt) {
		ptr->child = typeCalloc(struct tries,1);

		ptr = ptr->child;

		if (ptr == 0) {
			out_of_memory = TRUE;

			while ((ptr = savedptr) != 0) {
				savedptr = ptr->child;
				free(ptr);
			}

			return;
		}

		SET_TRY(ptr,txt);
		ptr->value = 0;
	}

	ptr->value = code;
	return;
}

/*
 * Expand a keycode into the string that it corresponds to, returning null if
 * no match was found, otherwise allocating a string of the result.
 */
char *_nc_expand_try(struct tries *tree, unsigned short code, size_t len)
{
	struct tries *ptr = tree;
	char *result = 0;

	if (code != 0) {
		while (ptr != 0) {
			if ((result = _nc_expand_try(ptr->child, code, len + 1)) != 0) {
				break;
			}
			if (ptr->value == code) {
				result = typeCalloc(char, len+2);
				break;
			}
			ptr = ptr->sibling;
		}
	}
	if (result != 0) {
		if ((result[len] = ptr->ch) == 0)
			*((unsigned char *)(result+len)) = 128;
#ifdef TRACE
		if (len == 0)
			_tracef("expand_key %s %s", _trace_key(code), _nc_visbuf(result));
#endif
	}
	return result;
}

/*
 * Remove a code from the specified tree, freeing the unused nodes.  Returns
 * true if the code was found/removed.
 */
int _nc_remove_key(struct tries **tree, unsigned short code)
{
	if (code == 0)
		return FALSE;
		
	while (*tree != 0) {
		if (_nc_remove_key(&(*tree)->child, code)) {
			return TRUE;
		}
		if ((*tree)->value == code) {
			if((*tree)->child) {
				/* don't cut the whole sub-tree */
				(*tree)->value = 0;
			} else {
				struct tries *to_free = *tree;
				*tree = (*tree)->sibling;
				free(to_free);
			}
			return TRUE;
		}
		tree = &(*tree)->sibling;
	}
	return FALSE;
}
