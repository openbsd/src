/*	$OpenBSD: keysym.c,v 1.3 2002/12/11 18:27:19 deraadt Exp $	*/
/*	$NetBSD: keysym.c,v 1.3 1999/02/08 11:08:23 hannken Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <dev/wscons/wsksymdef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "keysym.h"
#include "wsconsctl.h"

#define NUMKSYMS	(sizeof(ksym_tab_by_name)/sizeof(ksym_tab_by_name[0]))

static int first_time = 1;
static struct ksym ksym_tab_by_ksym[NUMKSYMS];

/* copied from dev/wscons/wskbdutil.c ... */

static const u_char latin1_to_upper[256] = {
/*      0  8  1  9  2  a  3  b  4  c  5  d  6  e  7  f               */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 1 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 1 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 2 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 2 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 3 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 3 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 4 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 4 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 5 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 5 */
	0x00,  'A',  'B',  'C',  'D',  'E',  'F',  'G',		/* 6 */
	 'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',		/* 6 */
	 'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',		/* 7 */
	 'X',  'Y',  'Z', 0x00, 0x00, 0x00, 0x00, 0x00,		/* 7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 8 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 8 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 9 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 9 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* a */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* a */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* b */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* b */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* c */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* c */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* d */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* d */
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,		/* e */
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,		/* e */
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0x00,		/* f */
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0x00,		/* f */
};

static int qcmp_name(const void *, const void *);
static int qcmp_ksym(const void *, const void *);
static int bcmp_name(const void *, const void *);
static int bcmp_ksym(const void *, const void *);

static void sort_ksym_tab(void);

static int
qcmp_name(const void *a, const void *b)
{
	return(strcmp(((struct ksym *) a)->name, ((struct ksym *) b)->name));
}

static int
qcmp_ksym(const void *a, const void *b)
{
	return(((struct ksym *) b)->value - ((struct ksym *) a)->value);
}

static int
bcmp_name(const void *a, const void *b)
{
	return(strcmp((char *) a, ((struct ksym *) b)->name));
}

static int
bcmp_ksym(const void *a, const void *b)
{
	return(((struct ksym *) b)->value - *((int *) a));
}

static void
sort_ksym_tab(void)
{
	int i;

	for (i = 0; i < NUMKSYMS; i++)
		ksym_tab_by_ksym[i] = ksym_tab_by_name[i];

	qsort(ksym_tab_by_name, NUMKSYMS, sizeof(struct ksym), qcmp_name);
	qsort(ksym_tab_by_ksym, NUMKSYMS, sizeof(struct ksym), qcmp_ksym);

	first_time = 0;
}

char *
ksym2name(int k)
{
	static char tmp[20];
	struct ksym *r;

	if (first_time)
		sort_ksym_tab();

	r = bsearch(&k, ksym_tab_by_ksym,
		    NUMKSYMS, sizeof(struct ksym), bcmp_ksym);

	if (r != NULL)
		return(r->name);
	else {
		snprintf(tmp, sizeof(tmp), "unknown_%d", k);
		return(tmp);
	}
}

int
name2ksym(char *n)
{
	int res;
	struct ksym *r;

	if (first_time)
		sort_ksym_tab();

	r = bsearch(n, ksym_tab_by_name,
		    NUMKSYMS, sizeof(struct ksym), bcmp_name);

	if (r != NULL)
		return(r->value);
	else if (sscanf(n, "unknown_%d", &res) == 1)
		return(res);
	else
		return(-1);
}

keysym_t
ksym_upcase(keysym_t ksym)
{
	if (ksym >= KS_f1 && ksym <= KS_f20)
		return(KS_F1 - KS_f1 + ksym);

	if (KS_GROUP(ksym) == KS_GROUP_Ascii && ksym <= 0xff &&
	    latin1_to_upper[ksym] != 0x00)
		return(latin1_to_upper[ksym]);

	return(ksym);
}
