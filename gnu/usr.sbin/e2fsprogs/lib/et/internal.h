/*
 * internal include file for com_err package
 */
#include "mit-sipb-copyright.h"
#ifndef __STDC__
#undef const
#define const
#endif

#include <errno.h>

#ifdef NEED_SYS_ERRLIST
extern char const * const sys_errlist[];
extern const int sys_nerr;
#endif

/* AIX and Ultrix have standard conforming header files. */
#if !defined(ultrix) && !defined(_AIX)
#ifdef __STDC__
void perror (const char *);
#endif
#endif
