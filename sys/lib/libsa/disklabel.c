/*	$OpenBSD: disklabel.c,v 1.7 2025/11/20 14:57:39 krw Exp $	*/
/*	$NetBSD: disklabel.c,v 1.3 1994/10/26 05:44:42 cgd Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)disklabel.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/param.h>
#include <sys/disklabel.h>
#include "stand.h"

#define offsetof(s, e) ((size_t)&((s *)0)->e)

char *
getdisklabel(const char *buf, struct disklabel *lp)
{
	struct disklabel *dlp, *elp;
	char *msg = NULL;
	size_t lpsz;

	/*
	 * XXX Only read the old smaller "skinny" label for now which
	 * has 16 partitions. offsetof() is used to carve struct disklabel.
	 * Later we'll add code to read and process the "fat" label with
	 * 52 partitions.
	 */
	lpsz = offsetof(struct disklabel, d_partitions[MAXPARTITIONS16]);

	elp = (struct disklabel *)(buf + DEV_BSIZE - lpsz);
	for (dlp = (struct disklabel *)buf; dlp <= elp;
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC) {
			if (msg == NULL)
				msg = "no disk label";
		} else if (dlp->d_npartitions > MAXPARTITIONS16 ||
			   dkcksum(dlp) != 0)
			msg = "disk label corrupted";
		else {
			memcpy(lp, dlp, lpsz);
			msg = NULL;
			break;
		}
	}
	return (msg);
}
