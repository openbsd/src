/*	$OpenBSD: file.h,v 1.15 2003/11/09 20:13:57 otto Exp $	*/

/*
 * file.h - definitions for file(1) program
 *
 * Copyright (c) Ian F. Darwin 1986-1995.
 * Software written by Ian F. Darwin and others;
 * maintained 1995-present by Christos Zoulas and others.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __file_h__
#define __file_h__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#elif defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#endif

#ifndef HOWMANY
# define HOWMANY 8192		/* how much of the file to look at */
#endif
#define MAXMAGIS 5000		/* max entries in /etc/magic */
#define MAXDESC	50		/* max leng of text description */
#define MAXstring 32		/* max leng of "string" types */

struct magic {
	short flag;		
#define INDIR	1		/* if '>(...)' appears,  */
#define	UNSIGNED 2		/* comparison is unsigned */
#define ADD	4		/* if '>&' appears,  */
	short cont_level;	/* level of ">" */
	struct {
		int8_t type;	/* byte short long */
		int32_t offset;	/* offset from indirection */
	} in;
	int32_t offset;		/* offset to magic number */
	unsigned char reln;	/* relation (0=eq, '>'=gt, etc) */
	int8_t type;		/* int, short, long or string. */
	char vallen;		/* length of string value, if any */
#define 			BYTE	1
#define				SHORT	2
#define				LONG	4
#define				STRING	5
#define				DATE	6
#define				BESHORT	7
#define				BELONG	8
#define				BEDATE	9
#define				LESHORT	10
#define				LELONG	11
#define				LEDATE	12
	union VALUETYPE {
		unsigned char b;
		unsigned short h;
		uint32_t l;
		char s[MAXstring];
		unsigned char hs[2];	/* 2 bytes of a fixed-endian "short" */
		unsigned char hl[4];	/* 2 bytes of a fixed-endian "long" */
	} value;		/* either number or string */
	uint32_t mask;	/* mask before comparison with value */
	char nospflag;		/* suppress space character */
	char desc[MAXDESC];	/* description */
};

extern int   apprentice(char *, int);
extern int   ascmagic(unsigned char *, int);
extern void  ckfputs(const char *, FILE *);
struct stat;
extern int   fsmagic(const char *, struct stat *);
extern int   is_compress(const unsigned char *, int *);
extern int   is_tar(unsigned char *, int);
extern void  mdump(struct magic *);
extern void  process(const char *, int);
extern void  showstr(FILE *, const char *, int);
extern int   softmagic(unsigned char *, int);
extern int   tryit(unsigned char *, int, int);
extern int   zmagic(unsigned char *, int);
extern void  ckfprintf(FILE *, const char *, ...);
extern uint32_t signextend(struct magic *, uint32_t);
extern int internatmagic(unsigned char *, int);
extern void tryelf(int, unsigned char *, int);


extern int errno;		/* Some unixes don't define this..	*/

extern char *progname;		/* the program name 			*/
extern char *magicfile;		/* name of the magic file		*/
extern int lineno;		/* current line number in magic file	*/

extern struct magic *magic;	/* array of magic entries		*/
extern int nmagic;		/* number of valid magic[]s 		*/


extern int debug;		/* enable debugging?			*/
extern int zflag;		/* process compressed files?		*/
extern int lflag;		/* follow symbolic links?		*/

extern int optind;		/* From getopt(3)			*/
extern char *optarg;

#if defined(sun) || defined(__sun__) || defined (__sun)
# if defined(__svr4) || defined (__SVR4) || defined(__svr4__)
#  define SOLARIS
# else
#  define SUNOS
# endif
#endif


#if !defined(__STDC__) || defined(SUNOS) || defined(__convex__)
extern int sys_nerr;
extern char *sys_errlist[];
#define strerror(e) \
	(((e) >= 0 && (e) < sys_nerr) ? sys_errlist[(e)] : "Unknown error")
#define strtoul(a, b, c)	strtol(a, b, c)
#endif

#ifndef MAXPATHLEN
#define	MAXPATHLEN	512
#endif

int	pipe2file(int, void *, size_t);
void	error(const char *, ...);

#endif /* __file_h__ */
