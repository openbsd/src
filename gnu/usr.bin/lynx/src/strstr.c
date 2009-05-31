/*
 * strstr.c -- return the offset of one string within another.
 *
 * Copyright (C) 1997 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Written by Philippe De Muyter <phdm@macqel.be>.  */

/*
 * NAME
 *
 * strstr -- locate first occurrence of a substring
 *
 * SYNOPSIS
 *
 * char *strstr (char *s1, char *s2)
 *
 * DESCRIPTION
 *
 * Locates the first occurrence in the string pointed to by S1 of the string
 * pointed to by S2.  Returns a pointer to the substring found, or a NULL
 * pointer if not found.  If S2 points to a string with zero length, the
 * function returns S1.
 *
 * BUGS
 *
 */

char *strstr(char *buf, char *sub)
{
    register char *bp;
    register char *sp;

    if (!*sub)
	return buf;
    while (*buf) {
	bp = buf;
	sp = sub;
	do {
	    if (!*sp)
		return buf;
	} while (*bp++ == *sp++);
	buf += 1;
    }
    return 0;
}
