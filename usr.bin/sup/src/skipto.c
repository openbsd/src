/*	$NetBSD: skipto.c,v 1.4 1997/06/17 21:38:25 christos Exp $	*/

/*
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator   or   Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the rights
 * to redistribute these changes.
 */
/************************************************************************
 *  skipover and skipto -- skip over characters in string
 *
 *  Usage:	p = skipto (string,charset);
 *		p = skipover (string,charset);
 *
 *  char *p,*charset,*string;
 *
 *  Skipto returns a pointer to the first character in string which
 *  is in the string charset; it "skips until" a character in charset.
 *  Skipover returns a pointer to the first character in string which
 *  is not in the string charset; it "skips over" characters in charset.
 ************************************************************************
 * HISTORY
 * 26-Jun-81  David Smith (drs) at Carnegie-Mellon University
 *	Skipover, skipto rewritten to avoid inner loop at expense of space.
 *
 * 20-Nov-79  Steven Shafer (sas) at Carnegie-Mellon University
 *	Skipover, skipto adapted for VAX from skip() and skipx() on the PDP-11
 *	(from Ken Greer).  The names are more mnemonic.
 *
 *	Sindex adapted for VAX from indexs() on the PDP-11 (thanx to Ralph
 *	Guggenheim).  The name has changed to be more like the index()
 *	and rindex() functions from Bell Labs; the return value (pointer
 *	rather than integer) has changed partly for the same reason,
 *	and partly due to popular usage of this function.
 */

#include "supcdefs.h"
#include "supextern.h"

static char tab[256] = {
	0};

char *skipto (string, charset)
char *string, *charset;
{
	char *setp, *strp;

	tab[0] = 1;		/* Stop on a null, too. */
	for (setp = charset;  *setp;  setp++)
		tab[(unsigned char) *setp] = 1;
	for (strp = string;  tab[(unsigned char) *strp]==0;  strp++)
		continue;
	for (setp = charset;  *setp;  setp++)
		tab[(unsigned char) *setp] = 0;
	return strp;
}

char *skipover (string, charset)
char *string, *charset;
{
	char *setp, *strp;

	tab[0] = 0;		/* Do not skip over nulls. */
	for (setp = charset;  *setp;  setp++)
		tab[(unsigned char) *setp] = 1;
	for (strp = string;  tab[(unsigned char) *strp];  strp++)
		continue;
	for (setp = charset;  *setp;  setp++)
		tab[(unsigned char) *setp] = 0;
	return strp;
}
