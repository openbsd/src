/*	$OpenBSD: tries.c,v 1.3 1998/07/23 21:20:07 millert Exp $	*/

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
 *  Author: Thomas E. Dickey <dickey@clark.net> 1997                        *
 ****************************************************************************/

/*
**	tries.c
**
**	Functions to manage the tree of partial-completions for keycodes.
**
*/

#include <curses.priv.h>

MODULE_ID("$From: tries.c,v 1.7 1998/02/11 12:13:57 tom Exp $")

#define SET_TRY(dst,src) if ((dst->ch = *src++) == 128) dst->ch = '\0'
#define CMP_TRY(a,b) ((a)? (a == b) : (b == 128))

void _nc_add_to_try(struct tries **tree, char *str, unsigned short code)
{
	static bool     out_of_memory = FALSE;
	struct tries    *ptr, *savedptr;
	unsigned char	*txt = (unsigned char *)str;

	if (txt == 0 || *txt == '\0' || out_of_memory || code == 0)
		return;

	if ((*tree) != 0) {
		ptr = savedptr = (*tree);

		for (;;) {
			unsigned char cmp = *txt;

			while (!CMP_TRY(ptr->ch, cmp)
			       &&  ptr->sibling != 0)
				ptr = ptr->sibling;
	
			if (CMP_TRY(ptr->ch, cmp)) {
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
