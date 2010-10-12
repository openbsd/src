/*	$OpenBSD: hexdump.h,v 1.9 2010/10/12 17:23:21 millert Exp $	*/
/*	$NetBSD: hexdump.h,v 1.7 2001/12/07 15:14:29 bjh21 Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
 *	from: @(#)hexdump.h	8.1 (Berkeley) 6/6/93
 */

typedef struct _pr {
	struct _pr *nextpr;		/* next print unit */
#define	F_ADDRESS	0x001		/* print offset */
#define	F_BPAD		0x002		/* blank pad */
#define	F_C		0x004		/* %_c */
#define	F_CHAR		0x008		/* %c */
#define	F_DBL		0x010		/* %[EefGf] */
#define	F_INT		0x020		/* %[di] */
#define	F_P		0x040		/* %_p */
#define	F_STR		0x080		/* %s */
#define	F_U		0x100		/* %_u */
#define	F_UINT		0x200		/* %[ouXx] */
#define	F_TEXT		0x400		/* no conversions */
	u_int flags;			/* flag values */
	int bcnt;			/* byte count */
	char *cchar;			/* conversion character */
	char *fmt;			/* printf format */
	char *nospace;			/* no whitespace version */
} PR;

typedef struct _fu {
	struct _fu *nextfu;		/* next format unit */
	struct _pr *nextpr;		/* next print unit */
#define	F_IGNORE	0x01		/* %_A */
#define	F_SETREP	0x02		/* rep count set, not default */
	u_int flags;			/* flag values */
	int reps;			/* repetition count */
	int bcnt;			/* byte count */
	char *fmt;			/* format string */
} FU;

typedef struct _fs {			/* format strings */
	struct _fs *nextfs;		/* linked list of format strings */
	struct _fu *nextfu;		/* linked list of format units */
	int bcnt;
} FS;

enum _vflag { ALL, DUP, FIRST, WAIT };	/* -v values */

extern int blocksize;			/* data block size */
extern int deprecated;			/* od compatibility */
extern FU *endfu;			/* format at end-of-data */
extern int exitval;			/* final exit value */
extern FS *fshead;			/* head of format strings list */
extern long length;			/* max bytes to read */
extern off_t skip;			/* bytes to skip */
extern char *iobuf;                            /* stdio I/O buffer */
extern size_t iobufsiz;                        /* size of stdio I/O buffer */
extern enum _vflag vflag;

void	 add(const char *);
void	 addfile(char *);
void	 badcnt(char *);
void	 badconv(char *);
void	 badfmt(const char *);
void	 badsfmt(void);
void	 bpad(PR *);
void	 conv_c(PR *, u_char *);
void	 conv_u(PR *, u_char *);
void	 display(void);
void	 doskip(const char *, int);
/*void	 err(const char *, ...);*/
void	*emalloc(int);
void	 escape(char *);
u_char	*get(void);
void	 newsyntax(int, char ***);
int	 next(char **);
void	 nomem(void);
void	 oldsyntax(int, char ***);
void	 rewrite(FS *);
int	 size(FS *);
void	 usage(void);
void	 oldusage(void);
