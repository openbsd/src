/*	$NetBSD: stand.h,v 1.1 1995/02/13 23:08:31 cgd Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)stand.h	8.1 (Berkeley) 6/11/93
 */

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/stat.h>
#include <lib/libsa/saioctl.h>
#include <lib/libsa/saerrno.h>

#ifndef NULL
#define	NULL	0
#endif

/* where values for lseek(2) */
#define	SEEK_SET	0	/* set file offset to offset */
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#define	SEEK_END	2	/* set file offset to EOF plus offset */

struct open_file {
	int		f_flags;	/* see F_* below */
	void		*f_devdata;	/* device specific data */
	void		*f_fsdata;	/* file system specific data */
};

#define	SOPEN_MAX	2
extern struct open_file files[SOPEN_MAX];
extern int nfsys;

/* f_flags values */
#define	F_READ		0x0001	/* file opened for reading */
#define	F_WRITE		0x0002	/* file opened for writing */
#define	F_RAW		0x0004	/* raw device open - no file system */
#define F_NODEV		0x0008	/* network open - no device */

#define isupper(c)	((c) >= 'A' && (c) <= 'Z')
#define tolower(c)	((c) - 'A' + 'a')
#define isspace(c)	((c) == ' ' || (c) == '\t')
#define isdigit(c)	((c) >= '0' && (c) <= '9')

int	devopen __P((struct open_file *f, const char *fname, char **file));
void	*alloc __P((unsigned size));
void	free __P((void *ptr, unsigned size));
struct	disklabel;
char	*getdisklabel __P((const char *buf, struct disklabel *lp));

void	printf __P((const char *, ...));
void	gets __P((char *));
__dead void	panic __P((const char *, ...))
			__attribute__((noreturn));
int	getchar __P((void));
int	exec __P((char *, char *, int));
int	open __P((const char *,int));
int	close __P((int));
int	read __P((int, void *, u_int));
int	write __P((int, void *, u_int));
    
int	nodev(), noioctl();
void	nullsys();

/* Machine dependent functions */
void	machdep_start __P((char *, int, char *, char *, char *));
int	machdep_exec __P((char *, char *, int));
int	getchar __P((void));
void	putchar __P((int));    
