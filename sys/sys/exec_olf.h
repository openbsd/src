/*	$OpenBSD: exec_olf.h,v 1.8 2001/06/22 14:11:00 deraadt Exp $	*/
/*
 * Copyright (c) 1996 Erik Theisen.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * OLF is a modified ELF that attempts to fix two serious shortcomings in
 * the SVR4 ABI.  Namely a lack of an operating system and strip tag.
 */

#ifndef _SYS_EXEC_OLF_H_
#define _SYS_EXEC_OLF_H_

#include <sys/exec_elf.h>

/* e_ident[] identification indexes */
#define OI_MAG0		EI_MAG0		/* file ID */
#define OI_MAG1		EI_MAG1		/* file ID */
#define OI_MAG2		EI_MAG2		/* file ID */
#define OI_MAG3		EI_MAG3		/* file ID */
#define OI_CLASS	EI_CLASS	/* file class */
#define OI_DATA		EI_DATA		/* data encoding */
#define OI_VERSION	EI_VERSION	/* OLF header version */
#define OI_OS		7		/* Operating system tag */
#define OI_DYNAMIC	8		/* Dynamic tag */
#define OI_STRIP	9		/* Strip tag */
#define OI_PAD		10		/* start of pad bytes */
#define OI_NIDENT	EI_NIDENT	/* Size of e_ident[] */

/* e_ident[] magic number */
#define OLFMAG0		ELFMAG0		/* e_ident[OI_MAG0] */
#define OLFMAG1		'O'		/* e_ident[OI_MAG1] */
#define OLFMAG2		ELFMAG2		/* e_ident[OI_MAG2] */
#define OLFMAG3		ELFMAG3		/* e_ident[OI_MAG3] */
#define OLFMAG		"\177OLF"	/* magic */
#define SOLFMAG		SELFMAG		/* size of magic */
 
/* e_ident[] file class */
#define OLFCLASSNONE	ELFCLASSNONE	/* invalid */
#define OLFCLASS32	ELFCLASS32	/* 32-bit objs */
#define OLFCLASS64	ELFCLASS64	/* 64-bit objs */
#define OLFCLASSNUM	ELFCLASSNUM	/* number of classes */
 
/* e_ident[] data encoding */
#define OLFDATANONE	ELFDATANONE	/* invalid */   
#define OLFDATA2LSB	ELFDATA2LSB	/* Little-Endian */
#define OLFDATA2MSB	ELFDATA2MSB	/* Big-Endian */
#define OLFDATANUM	ELFDATANUM	/* number of data encode defines */


/*
 * Please help make this list definative.
 */
/* e_ident[] system */
#define OOS_NULL	0		/* invalid */
#define OOS_OPENBSD	1		/* OpenBSD */
#define OOS_NETBSD	2		/* NetBSD */
#define OOS_FREEBSD	3		/* FreeBSD */
#define OOS_44BSD	4		/* 4.4BSD */
#define OOS_LINUX	5		/* Linux */
#define OOS_SVR4	6		/* AT&T System V Release 4 */
#define OOS_ESIX	7		/* esix UNIX */
#define OOS_SOLARIS	8		/* SunSoft Solaris */
#define OOS_IRIX	9		/* SGI IRIX */
#define OOS_SCO		10		/* SCO UNIX */
#define OOS_DELL	11		/* DELL SVR4 */
#define OOS_NCR		12		/* NCR SVR4 */
#define OOS_NUM		13		/* Number of systems */
/*
 * Lowercase and numbers ONLY.
 * No whitespace or punc.
 */
#define OOSN_NULL	"invalid"	/* invalid */
#define OOSN_OPENBSD	"openbsd"	/* OpenBSD */
#define OOSN_NETBSD	"netbsd"	/* NetBSD */
#define OOSN_FREEBSD	"freebsd"	/* FreeBSD */
#define OOSN_44BSD	"44bsd"		/* 4.4BSD */
#define OOSN_LINUX	"linux"		/* Linux */
#define OOSN_SVR4	"svr4"		/* AT&T System V Release 4 */
#define OOSN_ESIX	"esix"		/* esix UNIX */
#define OOSN_SOLARIS	"solaris"	/* SunSoft Solaris */
#define OOSN_IRIX	"irix"		/* SGI IRIX */
#define OOSN_SCO	"sco"		/* SCO UNIX */
#define OOSN_DELL	"dell"		/* DELL SVR4 */
#define OOSN_NCR	"ncr"		/* NCR SVR4 */
#define ONAMEV 		{ OOSN_NULL, OOSN_OPENBSD, OOSN_NETBSD, \
		 	  OOSN_FREEBSD, OOSN_44BSD, OOSN_LINUX, \
		 	  OOSN_SVR4, OOSN_ESIX, OOSN_SOLARIS, \
			  OOSN_IRIX, OOSN_SCO,  OOSN_DELL, \
			  OOSN_NCR, \
		 	0 }

/* e_ident[] dynamic */
#define ODYNAMIC_N	0		/* Statically linked  */
#define ODYNAMIC	1		/* Dynamically linked */

/* e_ident[] strip */
#define OSTRIP		0		/* Stripped */
#define OSTRIP_N	1		/* Not Stripped */

/* e_ident */
#define IS_OLF(ehdr) \
    ((ehdr).e_ident[OI_MAG0] == OLFMAG0 && \
    (ehdr).e_ident[OI_MAG1] == OLFMAG1 && \
    (ehdr).e_ident[OI_MAG2] == OLFMAG2 && \
    (ehdr).e_ident[OI_MAG3] == OLFMAG3)

/* The rest of the types and defines come from the ELF header file */
#endif /* _SYS_EXEC_OLF_H_ */
