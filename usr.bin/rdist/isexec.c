/*	$OpenBSD: isexec.c,v 1.4 1998/06/26 21:21:12 millert Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
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
 */
#ifndef lint
#if 0
static char RCSid[] = 
"$From: isexec.c,v 6.21 1994/04/01 23:44:10 mcooper Exp $";
#else
static char RCSid[] = 
"$OpenBSD: isexec.c,v 1.4 1998/06/26 21:21:12 millert Exp $";
#endif

static char sccsid[] = "@(#)client.c";

static char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */


#include "defs.h"

#if	EXE_TYPE == EXE_AOUT
/*
 * BSD style A.OUT
 */
#include <a.out.h>

static int _isexec(fd)
	int fd;
{
	struct exec ehdr;

	if ((read(fd, &ehdr, sizeof(ehdr)) == sizeof(ehdr)) && 
	    !N_BADMAG(ehdr))
		return(TRUE);
	else
		return(FALSE);
}
#endif /* EXE_AOUT */


#if	EXE_TYPE == EXE_ELF_AND_COFF || EXE_TYPE == EXE_ELF
/*
 * Elf
 */
#include <elf.h>
#define ISELF(h)	(h.e_type == ET_EXEC)
#endif	/* EXE_ELF_AND_COFF || EXE_ELF */

#if 	EXE_TYPE == EXE_ELF_AND_COFF || EXE_TYPE == EXE_COFF

/*
 * COFF
 */
#if defined(FILEHDR_H)
#include FILEHDR_H
#endif	/* FILEHDR_H */

#if !defined(ISCOFF)

/*
 * Stupid AIX
 */
#if defined(U802WRMAGIC) && defined(U802ROMAGIC) && defined(U802TOCMAGIC)
#define ISCOFF(x) (((x)==U802WRMAGIC) || ((x)==U802TOCMAGIC) || \
		   ((x)==U802TOCMAGIC))
#endif	/* U802... */
/*
 * Stupid Umax4.3
 */
#if 	defined(NS32GMAGIC) || defined(NS32SMAGIC)
#define ISCOFF(x) (((x)==NS32GMAGIC) || ((x)==NS32SMAGIC))
#endif 	/* NS32 ... */

#endif	/* ISCOFF */

#endif /* EXE_TYPE == EXE_ELF_AND_COFF || EXE_TYPE == EXE_COFF */

#if	EXE_TYPE == EXE_ELF_AND_COFF
/*
 * ELF and COFF
 */
typedef union {
    struct filehdr 	coffhdr;
    Elf32_Ehdr 		elfhdr;
} hdr_t;
#endif	/* EXE_TYPE == EXE_ELF_AND_COFF */

#if	EXE_TYPE == EXE_ELF
/*
 * Elf
 */
#include <elf.h>
typedef Elf32_Ehdr 	hdr_t;
#endif	/* EXE_TYPE == EXE_ELF */

#if	EXE_TYPE == EXE_COFF
/*
 * COFF
 */

#if	defined(FILEHDR_H)
#include FILEHDR_H
#endif	/* FILEHDR_H */

typedef struct filehdr 	hdr_t;
#endif	/* EXE_TYPE == EXE_COFF */

#if	EXE_TYPE == EXE_ELF_AND_COFF || EXE_TYPE == EXE_ELF || EXE_TYPE == EXE_COFF
/*
 * System V style COFF and System V R4 style ELF
 */
static int _isexec(fd)
	int fd;
{
	hdr_t hdr;

	if (read(fd, &hdr, sizeof(hdr)) == sizeof(hdr)) {
#if EXE_TYPE == EXE_ELF_AND_COFF
	    if (ISELF(hdr.elfhdr) || ISCOFF(hdr.coffhdr.f_magic))
		return(TRUE);
#endif
#if EXE_TYPE == EXE_ELF
	    if (ISELF(hdr))
		return(TRUE);
#endif
#if EXE_TYPE == EXE_COFF
	    if (ISCOFF(hdr.f_magic))
		return(TRUE);
#endif
	}

	return(FALSE);
}
#endif /* EXE_ELF_AND_COFF */


#if	EXE_TYPE == EXE_MACHO
/*
 * Mach-O format
 */

#if	defined(NEXTSTEP) && NEXTSTEP >= 3
#	include <mach-o/loader.h>
#else
#	include <sys/loader.h>
#endif	/* NEXTSTEP */

#ifndef MH_CIGAM
#define MH_CIGAM  	0xcefaedfe
#endif
#ifndef FAT_MAGIC
#define FAT_MAGIC 	0xcafebabe
#endif
#ifndef FAT_CIGAM
#define FAT_CIGAM 	0xbebafeca
#endif

static int _isexec(fd)
	int fd;
{
	struct mach_header ehdr;

	if ((read(fd, &ehdr, sizeof(ehdr)) == sizeof(ehdr)) && 
	    (ehdr.magic == MH_MAGIC || ehdr.magic == MH_CIGAM ||
	     ehdr.magic == FAT_MAGIC || ehdr.magic == FAT_CIGAM))
		return(TRUE);
	else
		return(FALSE);
}
#endif /* EXE_COFF */


#if	EXE_TYPE == EXE_HPEXEC
/*
 * HP 9000 executable format
 */

#ifdef hp9000s300

#include <a.out.h>
#define header exec
#define ISEXEC(a) ((a.file_type)==EXEC_MAGIC || (a.file_type)==SHARE_MAGIC || \
		   (a.file_type)==DEMAND_MAGIC)

#else	/* ! hp9000s300 */

#define ISEXEC(a) ((a)==EXEC_MAGIC || (a)==SHARE_MAGIC || (a)==DEMAND_MAGIC)
#include <filehdr.h>

#endif	/* hp9000s300 */

static int _isexec(fd)
	int fd;
{
	struct header ehdr;

	if ((read(fd, &ehdr, sizeof(ehdr)) == sizeof(ehdr)) &&
	    ISEXEC(ehdr.a_magic))
		return(TRUE);
	else
		return(FALSE);
}
#endif	/* EXE_HPEXEC */


#if	!defined(EXE_TYPE)
/*
 * Fake _isexec() call for unknown executable formats.
 */
static int _isexec(fd)
	/*ARGSUSED*/
	int fd;
{
	return(FALSE);
}
#endif	/* !defined(EXE_TYPE) */

/*
 * Determine whether 'file' is an executable or not.
 */
extern int isexec(file, statp)
	char *file;
	struct stat *statp;
{
	int fd, r;

	/*
	 * Must be a regular file that has some executable mode bit on
	 */
	if (!S_ISREG(statp->st_mode) ||
	    !(statp->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)))
		return(FALSE);

	if ((fd = open(file, O_RDONLY, 0)) < 0)
		return(FALSE);
	r = _isexec(fd);
	(void) close(fd);

	return(r);
}

