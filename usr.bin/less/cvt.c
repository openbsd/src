/*
 * Copyright (C) 1984-2012  Mark Nudelman
 * Modified for use with illumos by Garrett D'Amore.
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * Routines to convert text in various ways.  Used by search.
 */

#include "charset.h"
#include "less.h"

extern int utf_mode;

/*
 * Get the length of a buffer needed to convert a string.
 */
int
cvt_length(int len)
{
	if (utf_mode)
		/*
		 * Just copying a string in UTF-8 mode can cause it to grow
		 * in length.
		 * Four output bytes for one input byte is the worst case.
		 */
		len *= 4;
	return (len + 1);
}

/*
 * Allocate a chpos array for use by cvt_text.
 */
int *
cvt_alloc_chpos(int len)
{
	int i;
	int *chpos = ecalloc(sizeof (int), len);
	/* Initialize all entries to an invalid position. */
	for (i = 0; i < len; i++)
		chpos[i] = -1;
	return (chpos);
}

/*
 * Convert text.  Perform the transformations specified by ops.
 * Returns converted text in odst.  The original offset of each
 * odst character (when it was in osrc) is returned in the chpos array.
 */
void
cvt_text(char *odst, char *osrc, int *chpos, int *lenp, int ops)
{
	char *dst;
	char *edst = odst;
	char *src;
	char *src_end;
	wchar_t ch;
	int len;

	if (lenp != NULL)
		src_end = osrc + *lenp;
	else
		src_end = osrc + strlen(osrc);

	for (src = osrc, dst = odst; src < src_end; ) {
		int src_pos = src - osrc;
		int dst_pos = dst - odst;
		if ((len = mbtowc(&ch, src, src_end - src)) < 1)
			ch = L'\0';
		if ((ops & CVT_BS) && ch == '\b' && dst > odst) {
			src++;
			/* Delete backspace and preceding char. */
			do {
				dst--;
			} while (dst > odst && IS_UTF8_TRAIL(*dst));
		} else if ((ops & CVT_ANSI) && ch == ESC) {
			/* Skip to end of ANSI escape sequence. */
			src++;	/* skip the CSI start char */
			while (src < src_end)
				if (!is_ansi_middle(*src++))
					break;
		} else if (len < 1) {
			*dst++ = *src++;
			if (chpos != NULL)
				chpos[dst_pos] = src_pos;
		} else {
			src += len;
			/* Just copy the char to the destination buffer. */
			if ((ops & CVT_TO_LC) && iswupper(ch))
				ch = towlower(ch);
			dst += wctomb(dst, ch);
			/* Record the original position of the char. */
			if (chpos != NULL)
				chpos[dst_pos] = src_pos;
		}
		if (dst > edst)
			edst = dst;
	}
	if ((ops & CVT_CRLF) && edst > odst && edst[-1] == '\r')
		edst--;
	*edst = '\0';
	if (lenp != NULL)
		*lenp = edst - odst;
}
