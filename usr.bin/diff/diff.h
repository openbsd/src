/*	$OpenBSD: diff.h,v 1.11 2003/06/26 22:04:45 millert Exp $	*/

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

/*	diff.h	4.7	85/08/16	*/

/*
 * diff - common declarations
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>

/*
 * Output format options
 */
#define	D_NORMAL	0	/* Normal output */
#define	D_EDIT		-1	/* Editor script out */
#define	D_REVERSE	1	/* Reverse editor script */
#define	D_CONTEXT	2	/* Diff with context */
#define	D_UNIFIED	3	/* Unified context diff */
#define	D_IFDEF		4	/* Diff with merged #ifdef's */
#define	D_NREVERSE	5	/* Reverse ed script with numbered
				   lines and no trailing . */

extern int	aflag, bflag, hflag, iflag, lflag, rflag, sflag, tflag, wflag;
extern char	*start, *ifdefname;
extern int	opt, wantelses, inifdef, context, status, anychange;
extern char	*tempfiles[], **diffargv;
extern char	*file1, *file2, *efile1, *efile2;
extern struct	stat stb1, stb2;
extern const char *diffh, *diff, *pr;

void	*emalloc(size_t);
void	*erealloc(void *, size_t);
char	*splice(char *, char *);
char	*copytemp(const char *, int);
void	diffdir(char **);
void	diffreg(void);
int	max(int, int);
int	min(int, int);
__dead void error(const char *, ...);
__dead void errorx(const char *, ...);
__dead void done(int);
