/*	$Id: mk_cmds_defs.h,v 1.1.1.1 1995/12/14 06:52:48 tholo Exp $	*/

#include <stdio.h>
#include <string.h>

#ifdef __STDC__

#define PROTOTYPE(p) p
typedef void * pointer;

#else

#define const
#define volatile
#define PROTOTYPE(p) ()
typedef char * pointer;

#endif /* not __STDC__ */

#if defined(__GNUC__)
#define LOCAL_ALLOC(x) __builtin_alloca(x)
#define LOCAL_FREE(x)
#else
#if defined(vax)
#define LOCAL_ALLOC(x) alloca(x)
#define LOCAL_FREE(x)
extern pointer alloca PROTOTYPE((unsigned));
#else
#if defined(__HIGHC__)	/* Barf! */
pragma on(alloca);
#define LOCAL_ALLOC(x) alloca(x)
#define LOCAL_FREE(x)
extern pointer alloca PROTOTYPE((unsigned));
#else
/* no alloca? */
#define LOCAL_ALLOC(x) malloc(x)
#define LOCAL_FREE(x) free(x)
#endif
#endif
#endif				/* LOCAL_ALLOC stuff */
