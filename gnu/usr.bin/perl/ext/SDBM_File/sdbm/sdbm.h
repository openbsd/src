/*
 * sdbm - ndbm work-alike hashed database library
 * based on Per-Ake Larson's Dynamic Hashing algorithms. BIT 18 (1978).
 * author: oz@nexus.yorku.ca
 * status: public domain. 
 */
#define DBLKSIZ 4096
#define PBLKSIZ 1024
#define PAIRMAX 1008			/* arbitrary on PBLKSIZ-N */
#define SPLTMAX	10			/* maximum allowed splits */
					/* for a single insertion */
#define DIRFEXT	".dir"
#define PAGFEXT	".pag"

typedef struct {
	int dirf;		       /* directory file descriptor */
	int pagf;		       /* page file descriptor */
	int flags;		       /* status/error flags, see below */
	long maxbno;		       /* size of dirfile in bits */
	long curbit;		       /* current bit number */
	long hmask;		       /* current hash mask */
	long blkptr;		       /* current block for nextkey */
	int keyptr;		       /* current key for nextkey */
	long blkno;		       /* current page to read/write */
	long pagbno;		       /* current page in pagbuf */
	char pagbuf[PBLKSIZ];	       /* page file block buffer */
	long dirbno;		       /* current block in dirbuf */
	char dirbuf[DBLKSIZ];	       /* directory file block buffer */
} DBM;

#define DBM_RDONLY	0x1	       /* data base open read-only */
#define DBM_IOERR	0x2	       /* data base I/O error */

/*
 * utility macros
 */
#define sdbm_rdonly(db)		((db)->flags & DBM_RDONLY)
#define sdbm_error(db)		((db)->flags & DBM_IOERR)

#define sdbm_clearerr(db)	((db)->flags &= ~DBM_IOERR)  /* ouch */

#define sdbm_dirfno(db)	((db)->dirf)
#define sdbm_pagfno(db)	((db)->pagf)

typedef struct {
	char *dptr;
	int dsize;
} datum;

extern datum nullitem;

#ifdef __STDC__
#define proto(p) p
#else
#define proto(p) ()
#endif

/*
 * flags to sdbm_store
 */
#define DBM_INSERT	0
#define DBM_REPLACE	1

/*
 * ndbm interface
 */
extern DBM *sdbm_open proto((char *, int, int));
extern void sdbm_close proto((DBM *));
extern datum sdbm_fetch proto((DBM *, datum));
extern int sdbm_delete proto((DBM *, datum));
extern int sdbm_store proto((DBM *, datum, datum, int));
extern datum sdbm_firstkey proto((DBM *));
extern datum sdbm_nextkey proto((DBM *));

/*
 * other
 */
extern DBM *sdbm_prep proto((char *, char *, int, int));
extern long sdbm_hash proto((char *, int));

#ifndef SDBM_ONLY
#define dbm_open sdbm_open;
#define dbm_close sdbm_close;
#define dbm_fetch sdbm_fetch;
#define dbm_store sdbm_store;
#define dbm_delete sdbm_delete;
#define dbm_firstkey sdbm_firstkey;
#define dbm_nextkey sdbm_nextkey;
#define dbm_error sdbm_error;
#define dbm_clearerr sdbm_clearerr;
#endif

/* Most of the following is stolen from perl.h. */
#ifndef H_PERL  /* Include guard */

/*
 * The following contortions are brought to you on behalf of all the
 * standards, semi-standards, de facto standards, not-so-de-facto standards
 * of the world, as well as all the other botches anyone ever thought of.
 * The basic theory is that if we work hard enough here, the rest of the
 * code can be a lot prettier.  Well, so much for theory.  Sorry, Henry...
 */

#include <errno.h>
#ifdef HAS_SOCKET
#   ifdef I_NET_ERRNO
#     include <net/errno.h>
#   endif
#endif

#ifdef MYMALLOC
#   ifdef HIDEMYMALLOC
#	define malloc Mymalloc
#	define realloc Myremalloc
#	define free Myfree
#   endif
#   define safemalloc malloc
#   define saferealloc realloc
#   define safefree free
#endif

#if defined(__STDC__) || defined(_AIX) || defined(__stdc__) || defined(__cplusplus)
# define STANDARD_C 1
#endif

#include <stdio.h>
#include <ctype.h>
#include <setjmp.h>

#ifdef I_UNISTD
#include <unistd.h>
#endif

#ifndef MSDOS
#   ifdef PARAM_NEEDS_TYPES
#	include <sys/types.h>
#   endif
#   include <sys/param.h>
#endif

#ifndef _TYPES_		/* If types.h defines this it's easy. */
#   ifndef major		/* Does everyone's types.h define this? */
#	include <sys/types.h>
#   endif
#endif

#include <sys/stat.h>

#ifndef SEEK_SET
# ifdef L_SET
#  define SEEK_SET	L_SET
# else
#  define SEEK_SET	0  /* Wild guess. */
# endif
#endif

/* Use all the "standard" definitions? */
#if defined(STANDARD_C) && defined(I_STDLIB)
#   include <stdlib.h>
#endif /* STANDARD_C */

#define MEM_SIZE Size_t

#ifdef I_STRING
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef I_MEMORY
#include <memory.h>
#endif

#if defined(mips) && defined(ultrix) && !defined(__STDC__)
#   undef HAS_MEMCMP
#endif

#ifdef HAS_MEMCPY
#  if !defined(STANDARD_C) && !defined(I_STRING) && !defined(I_MEMORY)
#    ifndef memcpy
        extern char * memcpy _((char*, char*, int));
#    endif
#  endif
#else
#   ifndef memcpy
#	ifdef HAS_BCOPY
#	    define memcpy(d,s,l) bcopy(s,d,l)
#	else
#	    define memcpy(d,s,l) my_bcopy(s,d,l)
#	endif
#   endif
#endif /* HAS_MEMCPY */

#ifdef HAS_MEMSET
#  if !defined(STANDARD_C) && !defined(I_STRING) && !defined(I_MEMORY)
#    ifndef memset
	extern char *memset _((char*, int, int));
#    endif
#  endif
#  define memzero(d,l) memset(d,0,l)
#else
#   ifndef memzero
#	ifdef HAS_BZERO
#	    define memzero(d,l) bzero(d,l)
#	else
#	    define memzero(d,l) my_bzero(d,l)
#	endif
#   endif
#endif /* HAS_MEMSET */

#ifdef HAS_MEMCMP
#  if !defined(STANDARD_C) && !defined(I_STRING) && !defined(I_MEMORY)
#    ifndef memcmp
	extern int memcmp _((char*, char*, int));
#    endif
#  endif
#else
#   ifndef memcmp
#	define memcmp 	my_memcmp
#   endif
#endif /* HAS_MEMCMP */

/* we prefer bcmp slightly for comparisons that don't care about ordering */
#ifndef HAS_BCMP
#   ifndef bcmp
#	define bcmp(s1,s2,l) memcmp(s1,s2,l)
#   endif
#endif /* HAS_BCMP */

#ifdef I_NETINET_IN
#   include <netinet/in.h>
#endif

#endif /* Include guard */
