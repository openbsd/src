/*	$OpenBSD: libc.h,v 1.3 1997/04/01 07:35:06 todd Exp $	*/

/*
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator   or   Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the rights
 * to redistribute these changes.
 */
/*
 **********************************************************************
 * HISTORY
 * 
 * 30-Apr-88  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added pathof() extern.
 *
 * 01-Dec-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added getname() extern.
 *
 * 29-Nov-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added lseek() extern.
 *
 * 02-Nov-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added salloc() extern.
 *
 * 14-Aug-81  Mike Accetta (mja) at Carnegie-Mellon University
 *	Created.
 *
 **********************************************************************
 */

#ifndef	_LIBC_H_
#define	_LIBC_H_ 1

#ifndef _TYPES_
#include <sys/types.h>
#endif	/* _TYPES_ */

#ifndef	_SYSENT_H_
#include <sysent.h>
#endif	/* _SYSENT_H_ */

#ifndef	FILE
#include <stdio.h>
#endif	/* FILE */

#ifndef	_STRINGS_H_
#include <strings.h>
#endif	/* _STRINGS_H_ */

#ifndef	_TIME_H_
#include <time.h>
#endif	/* _TIME_H_ */

/*  CMU stdio additions */
#if defined(__STDC__)
extern FILE *fopenp(const char*, const char*, char*, char*);
extern FILE *fwantread(const char*, const char*, const char*, const char*);
extern FILE *fwantwrite(const char*, const char*, const char*, const char*,
			int);
#else	/* __STDC__ */
extern FILE *fopenp();
extern FILE *fwantread();
extern FILE *fwantwrite();
#endif	/* __STDC__ */

/* CMU string routines */
#if defined(__STDC__)
extern char* foldup(char*, const char*);
extern char* folddown(char*, const char*);
extern char* sindex(const char*, const char*);
extern char _argbreak;
extern char* getstr(const char*, char*, char*);
extern int getstab(const char*, const char**, const char*);
extern int getsearch(const char*, const char**, const char*);
extern char* strarg(const char**, const char*, const char*, char*, char*);
extern int stabarg(const char**, const char*, const char*, const char**,
		   const char*);
extern int searcharg(const char**, const char*, const char*, const char**,
		     const char*);
extern int getint(const char*, int, int, int);
extern int intarg(const char**, const char*, const char*, int, int, int);
extern long getlong(const char*, long, long, long);
extern long longarg(const char**, const char*, const char*, long, long, long);
extern short getshort(const char*, short, short, short);
extern short shortarg(const char**, const char*, const char*,
		      short, short, short);
extern float getfloat(const char*, float, float, float);
extern float floatarg(const char**, const char*, const char*,
		      float, float, float);
extern double getdouble(const char*, double, double, double);
extern double doublearg(const char**, const char*, const char*,
			double, double, double);
extern unsigned int getoct(const char*, unsigned int, unsigned int,
			   unsigned int);
extern unsigned int octarg(const char**, const char*, const char*,
			   unsigned int, unsigned int, unsigned int);
extern unsigned int gethex(const char*, unsigned int, unsigned int,
			   unsigned int);
extern unsigned int hexarg(const char**, const char*, const char*,
			   unsigned int, unsigned int, unsigned int);
extern unsigned int atoh(const char*);
extern char *concat(const char*, int, ...);
#else	/* __STDC__ */
extern char *foldup(), *folddown();
extern char *sindex(), *skipto(), *skipover(), *nxtarg();
extern char *getstr(), *strarg();
extern long getlong(), longarg();
extern short getshort(), shortarg();
extern float getfloat(), floatarg();
extern double getdouble(), doublearg();
extern unsigned int getoct(), octarg(), gethex(), hexarg();
extern unsigned int atoo(), atoh();
extern char *salloc();
extern char *concat();
#endif	/* __STDC__ */

/* CMU library routines */
#if defined(__STDC__)
extern char *getname(int);
extern char *pathof(char *);
extern char *errmsg(int);
#else	/* __STDC__ */
extern char *getname();
extern char *pathof();
extern char *errmsg();
#endif	/* __STDC__ */

/*  CMU time additions */
#if defined(__STDC__)
extern long gtime(const struct tm*);
extern long atot(const char*);
#else	/* __STDC__ */
extern long gtime();
extern long atot();
#endif	/* __STDC__ */

/* 4.3 BSD standard library routines; taken from man(3) */
#if defined(__STDC__)
#if 0
typedef int (*PFI)();
#endif
#if defined(c_plusplus)
typedef int (*PFI2)(...);
#endif /* c_plusplus */
#if 0
extern void abort(void);
extern int abs(int);
extern double atof(const char *);
extern int atoi(const char *);
extern long atol(const char *);
extern void bcopy(const void *, void *, int);
extern int bcmp(const void *, const void *, int);
extern void bzero(void *, int);
extern int ffs(int);
extern char *crypt(const char *, const char *);
extern void setkey(char *);
extern void encrypt(char *, int);
extern char *ecvt(double, int, int *, int *);
extern char *fcvt(double, int, int *, int *);
extern char *gcvt(double, int, char *);
extern int execl(const char *, ...);
extern int execv(const char *, const char **);
extern int execle(const char *, ...);
extern int exect(const char *, const char **, const char **);
extern void exit(int);
extern char *getenv(const char *);
extern char *getlogin(void);
extern int getopt(int, const char **, const char *);
extern char *getpass(const char *);
extern char *getusershell(void);
extern void setusershell(void);
extern void endusershell(void);
extern char *getwd(char *);
extern int initgroups(const char *, gid_t);
extern void *malloc(unsigned);
extern void free(void *);
extern void *realloc(void *, unsigned);
extern void *calloc(unsigned, unsigned);
extern void *alloca(int);
extern char *mktemp(char *);
extern int mkstemp(char *);
extern void monitor(PFI, PFI, short *, int, int);
extern void monstartup(PFI, PFI);
extern void moncontrol(int);
extern int pause(void);
#if defined(c_plusplus)
extern void qsort(void *, int, int, PFI2);
#else /* c_plusplus */
extern void qsort(void *, int, int, PFI);
#endif /* c_plusplus */
extern long random(void);
extern int srandom(int);
extern void *initstate(unsigned, void *, int);
extern void *setstate(void *);
extern int rcmd(char **, int, const char *, const char *, const char *, int);
extern int rresvport(int *);
extern int ruserok(char *, int, const char *, const char *);
extern char *re_comp(char *);
extern int re_exec(char *);
extern int rexec(char **, int, const char *, const char *, const char *,
		 int *);
extern int setuid(uid_t);
extern int seteuid(uid_t);
extern int setruid(uid_t);
extern int setgid(gid_t);
extern int setegid(gid_t);
extern int setrgid(gid_t);
extern void sleep(unsigned);
extern void swab(void *, void *, int);
extern int system(const char *);
extern char *ttyname(int);
extern int isatty(int);
extern int ttyslot(void);
extern unsigned ualarm(unsigned, unsigned);
extern void usleep(unsigned);
#endif
#else	/* __STDC__ */
extern double atof();
extern long atol();
extern char *crypt();
extern char *ecvt();
extern char *fcvt();
extern char *gcvt();
extern char *getenv();
extern char *getlogin();
extern char *getpass();
extern char *getusershell();
extern char *getwd();
extern char *malloc();
extern char *realloc();
extern char *calloc();
extern char *alloca();
extern char *mktemp();
extern long random();
extern char *initstate();
extern char *setstate();
extern char *re_comp();
extern char *ttyname();
extern unsigned ualarm();
#endif	/* __STDC__ */
#endif	/* not _LIBC_H_ */
