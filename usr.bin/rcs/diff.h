/*	$OpenBSD: diff.h,v 1.7 2007/07/03 00:56:23 ray Exp $	*/
/*
 * Copyright (C) Caldera International Inc.  2001-2002.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code and documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed or owned by Caldera
 *	International, Inc.
 * 4. Neither the name of Caldera International, Inc. nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE FOR ANY DIRECT,
 * INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2004 Jean-Francois Brousseau.  All rights reserved.
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
 *	@(#)diffreg.c   8.1 (Berkeley) 6/6/93
 */

#ifndef RCS_DIFF_H
#define RCS_DIFF_H

#include <sys/queue.h>

#include <regex.h>

#include "buf.h"
#include "rcs.h"

/*
 * Output format options
 */
#define	D_NORMAL	0	/* Normal output */
#define	D_CONTEXT	1	/* Diff with context */
#define	D_UNIFIED	2	/* Unified context diff */
#define	D_IFDEF		3	/* Diff with merged #ifdef's */
#define	D_BRIEF		4	/* Say if the files differ */
#define	D_RCSDIFF	5       /* Reverse editor output: RCS format */

/*
 * Command line flags
 */
#define	D_FORCEASCII	0x01	/* Treat file as ascii regardless of content */
#define	D_FOLDBLANKS	0x02	/* Treat all white space as equal */
#define	D_MINIMAL	0x04	/* Make diff as small as possible */
#define	D_IGNORECASE	0x08	/* Case-insensitive matching */
#define	D_PROTOTYPE	0x10	/* Display C function prototype */
#define	D_EXPANDTABS	0x20	/* Expand tabs to spaces */
#define	D_IGNOREBLANKS	0x40	/* Ignore white space changes */

/*
 * Status values for diffreg() return values
 */
#define	D_SAME		0	/* Files are the same */
#define	D_DIFFER	1	/* Files are different */
#define	D_ERROR		2	/* An error occurred */

/*
 * Status values for rcs_diff3() return values
 */
#define D_OVERLAPS	1	/* Overlaps during merge */

struct rcs_lines;

BUF		*rcs_diff3(RCSFILE *, char *, RCSNUM *, RCSNUM *, int);
BUF		*merge_diff3(char **, int);
void		diff_output(const char *, ...);
int		diffreg(const char *, const char *, BUF *, int);
int		ed_patch_lines(struct rcs_lines *, struct rcs_lines *);

extern int       diff_context;
extern int       diff_format;
extern int	 diff3_conflicts;
extern char	*diff_file, *diff_ignore_pats;
extern char	 diffargs[512]; /* XXX */
extern BUF	*diffbuf;
extern RCSNUM	*diff_rev1;
extern RCSNUM	*diff_rev2;
extern regex_t	*diff_ignore_re;

#endif	/* RCS_DIFF_H */
