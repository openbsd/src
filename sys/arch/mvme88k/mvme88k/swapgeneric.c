/*	$OpenBSD: swapgeneric.c,v 1.4 1998/12/15 05:11:02 smurph Exp $ */

/*-
 * Copyright (c) 1994
 *      The Regents of the University of California.  All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *      @(#)swapgeneric.c       8.2 (Berkeley) 3/21/94
 */

/*
 * fake swapgeneric.c -- should do this differently.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <machine/disklabel.h>

int (*mountroot) __P((void)) = NULL;	/* tells autoconf.c that we are "generic" */

dev_t	rootdev = NODEV;
dev_t	dumpdev = NODEV;

struct	swdevt swdevt[] = {
	{ makedev(4, 0*MAXPARTITIONS+1), 0, 0 },	/* sd0b */
	{ makedev(4, 1*MAXPARTITIONS+1), 0, 0 },	/* sd1b */
	{ makedev(4, 2*MAXPARTITIONS+1), 0, 0 },	/* sd2b */
	{ makedev(4, 3*MAXPARTITIONS+1), 0, 0 },	/* sd3b */
	{ makedev(4, 4*MAXPARTITIONS+1), 0, 0 },	/* sd4b */
	{ makedev(4, 5*MAXPARTITIONS+1), 0, 0 },	/* sd5b */
	{ makedev(4, 6*MAXPARTITIONS+1), 0, 0 },	/* sd6b */
	{ makedev(4, 7*MAXPARTITIONS+1), 0, 0 },	/* sd7b */
	{ NODEV, 0, 0 }
};
