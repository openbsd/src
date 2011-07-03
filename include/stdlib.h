/*	$OpenBSD: stdlib.h,v 1.49 2011/07/03 18:51:01 jsg Exp $	*/
/*	$NetBSD: stdlib.h,v 1.25 1995/12/27 21:19:08 jtc Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	@(#)stdlib.h	5.13 (Berkeley) 6/4/91
 */

#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <sys/cdefs.h>
#include <machine/_types.h>
#if __BSD_VISIBLE	/* for quad_t, etc. (XXX - use protected types) */
#include <sys/types.h>
#endif

#ifndef	_SIZE_T_DEFINED_
#define	_SIZE_T_DEFINED_
typedef	__size_t	size_t;
#endif

/* in C++, wchar_t is a built-in type */
#if !defined(_WCHAR_T_DEFINED_) && !defined(__cplusplus)
#define _WCHAR_T_DEFINED_
typedef	__wchar_t	wchar_t;
#endif

typedef struct {
	int quot;		/* quotient */
	int rem;		/* remainder */
} div_t;

typedef struct {
	long quot;		/* quotient */
	long rem;		/* remainder */
} ldiv_t;

#if __ISO_C_VISIBLE >= 1999
typedef struct {
	long long quot;		/* quotient */
	long long rem;		/* remainder */
} lldiv_t;
#endif

#if __BSD_VISIBLE
typedef struct {
	quad_t quot;		/* quotient */
	quad_t rem;		/* remainder */
} qdiv_t;
#endif


#ifndef	NULL
#ifdef 	__GNUG__
#define NULL	__null
#elif defined(__cplusplus)
#define	NULL	0L
#else
#define	NULL	((void *)0)
#endif
#endif

#define	EXIT_FAILURE	1
#define	EXIT_SUCCESS	0

#define	RAND_MAX	0x7fffffff

extern size_t	__mb_cur_max;
#define	MB_CUR_MAX	__mb_cur_max

#include <sys/cdefs.h>

/*
 * Some header files may define an abs macro.
 * If defined, undef it to prevent a syntax error and issue a warning.
 */
#ifdef abs
#undef abs
#warning abs macro collides with abs() prototype, undefining
#endif

__BEGIN_DECLS
__dead void	 abort(void);
int	 abs(int);
int	 atexit(void (*)(void));
double	 atof(const char *);
int	 atoi(const char *);
long	 atol(const char *);
void	*bsearch(const void *, const void *, size_t, size_t,
	    int (*)(const void *, const void *));
void	*calloc(size_t, size_t);
div_t	 div(int, int);
char	*ecvt(double, int, int *, int *);
__dead void	 exit(int);
__dead void	 _Exit(int);
char	*fcvt(double, int, int *, int *);
void	 free(void *);
char	*gcvt(double, int, char *);
char	*getenv(const char *);
long	 labs(long);
ldiv_t	 ldiv(long, long);
void	*malloc(size_t);
int	 posix_memalign(void **, size_t, size_t);
void	 qsort(void *, size_t, size_t, int (*)(const void *, const void *));
int	 rand(void);
void	*realloc(void *, size_t);
void	 srand(unsigned);
double	 strtod(const char *, char **);
float	 strtof(const char *, char **);
long	 strtol(const char *, char **, int);
long double
	 strtold(const char *, char **);
unsigned long
	 strtoul(const char *, char **, int);
int	 system(const char *);

/* these are currently just stubs */
int	 mblen(const char *, size_t);
size_t	 mbstowcs(wchar_t *, const char *, size_t);
int	 wctomb(char *, wchar_t);
int	 mbtowc(wchar_t *, const char *, size_t);
size_t	 wcstombs(char *, const wchar_t *, size_t);

/*
 * IEEE Std 1003.1c-95, also adopted by X/Open CAE Spec Issue 5 Version 2
 */
#if __BSD_VISIBLE || __POSIX_VISIBLE >= 199506 || __XPG_VISIBLE >= 500 || \
	defined(_REENTRANT)
int	 rand_r(unsigned int *);
#endif

#if __BSD_VISIBLE || __XPG_VISIBLE >= 400
double	 drand48(void);
double	 erand48(unsigned short[3]);
long	 jrand48(unsigned short[3]);
void	 lcong48(unsigned short[7]);
long	 lrand48(void);
long	 mrand48(void);
long	 nrand48(unsigned short[3]);
unsigned short *seed48(unsigned short[3]);
void	 srand48(long);

int	 putenv(char *);
#endif

#if __BSD_VISIBLE || __XPG_VISIBLE >= 420
long	 a64l(const char *);
char	*l64a(long);

char	*initstate(unsigned int, char *, size_t)
		__attribute__((__bounded__ (__string__,2,3)));
long	 random(void);
char	*setstate(const char *);
void	 srandom(unsigned int);

int	 mkstemp(char *);
char	*mktemp(char *);

char	*realpath(const char *, char *);

int	 setkey(const char *);

int	 ttyslot(void);

void	*valloc(size_t);		/* obsoleted by malloc() */
#endif /* __BSD_VISIBLE || __XPG_VISIBLE >= 420 */

/*
 * ISO C99
 */
#if __ISO_C_VISIBLE >= 1999
long long
	 atoll(const char *);
long long
	 llabs(long long);
lldiv_t
	 lldiv(long long, long long);
long long
	 strtoll(const char *, char **, int);
unsigned long long
	 strtoull(const char *, char **, int);
#endif

/*
 * The Open Group Base Specifications, Issue 6; IEEE Std 1003.1-2001 (POSIX)
 */
#if __BSD_VISIBLE || __POSIX_VISIBLE >= 200112 || __XPG_VISIBLE >= 600
int	 setenv(const char *, const char *, int);
int	 unsetenv(const char *);
#endif

#if __BSD_VISIBLE
void	*alloca(size_t); 

char	*getbsize(int *, long *);
char	*cgetcap(char *, const char *, int);
int	 cgetclose(void);
int	 cgetent(char **, char **, const char *);
int	 cgetfirst(char **, char **);
int	 cgetmatch(char *, const char *);
int	 cgetnext(char **, char **);
int	 cgetnum(char *, const char *, long *);
int	 cgetset(const char *);
int	 cgetusedb(int);
int	 cgetstr(char *, const char *, char **);
int	 cgetustr(char *, const char *, char **);

int	 daemon(int, int);
char	*devname(int, mode_t);
int	 getloadavg(double [], int);

void	 cfree(void *);

#ifndef _GETOPT_DEFINED_
#define _GETOPT_DEFINED_
int	 getopt(int, char * const *, const char *);
extern	 char *optarg;			/* getopt(3) external variables */
extern	 int opterr, optind, optopt, optreset;
int	 getsubopt(char **, char * const *, char **);
extern	 char *suboptarg;		/* getsubopt(3) external variable */
#endif /* _GETOPT_DEFINED_ */

char	*mkdtemp(char *);
int	 mkstemps(char *, int);

int	 heapsort(void *, size_t, size_t, int (*)(const void *, const void *));
int	 mergesort(void *, size_t, size_t, int (*)(const void *, const void *));
int	 radixsort(const unsigned char **, int, const unsigned char *,
	    unsigned);
int	 sradixsort(const unsigned char **, int, const unsigned char *,
	    unsigned);

void	 srandomdev(void);
long long
	 strtonum(const char *, long long, long long, const char **);

void	 setproctitle(const char *, ...)
	__attribute__((__format__ (__printf__, 1, 2)));

quad_t	 qabs(quad_t);
qdiv_t	 qdiv(quad_t, quad_t);
quad_t	 strtoq(const char *, char **, int);
u_quad_t strtouq(const char *, char **, int);

u_int32_t arc4random(void);
void	arc4random_stir(void);
void	arc4random_addrandom(unsigned char *, int)
	__attribute__((__bounded__ (__string__,1,2)));
u_int32_t arc4random_uniform(u_int32_t);
void arc4random_buf(void *, size_t)
	__attribute__((__bounded__ (__string__,1,2)));

#endif /* __BSD_VISIBLE */

__END_DECLS

#endif /* _STDLIB_H_ */
