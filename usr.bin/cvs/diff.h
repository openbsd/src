/*	$OpenBSD: diff.h,v 1.22 2010/07/28 21:19:30 nicm Exp $	*/
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
#ifndef DIFF_H
#define DIFF_H
#define CVS_DIFF_DEFCTX	3	/* default context length */

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
#define	D_BINARY	2	/* Binary files are different */
#define	D_COMMON	3	/* Subdirectory common to both dirs */
#define	D_ONLY		4	/* Only exists in one directory */
#define	D_MISMATCH1	5	/* path1 was a dir, path2 a file */
#define	D_MISMATCH2	6	/* path1 was a file, path2 a dir */
#define	D_ERROR		7	/* An error occurred */
#define	D_SKIPPED1	8	/* path1 was a special file */
#define	D_SKIPPED2	9	/* path2 was a special file */

void		cvs_merge_file(struct cvs_file *, int);
void		diff_output(const char *, ...);
int		diffreg(const char *, const char *, int, int, BUF *, int);
int		ed_patch_lines(struct rcs_lines *, struct rcs_lines *);

extern int       diff_format;
extern int	 diff_context;
extern int	 diff3_conflicts;
extern int	 diff_aflag;
extern int	 diff_bflag;
extern int	 diff_dflag;
extern int	 diff_iflag;
extern int	 diff_pflag;
extern int	 diff_wflag;
extern char	*diff_file;
extern char	 diffargs[512]; /* XXX */
extern BUF	*diffbuf;
extern RCSNUM	*diff_rev1;
extern RCSNUM	*diff_rev2;
extern RCSNUM	*d3rev1;
extern RCSNUM	*d3rev2;

#endif	/* DIFF_H */
