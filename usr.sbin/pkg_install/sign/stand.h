/* $OpenBSD: stand.h,v 1.4 2002/02/17 19:42:38 millert Exp $ */

/* provided to cater for BSD idiosyncrasies */

#if (defined(__unix__) || defined(unix)) && !defined(USG)
#include <sys/param.h>
#endif

#if defined(BSD4_4)
#include <err.h>
#else
extern void set_program_name(const char * name);
extern void warn(const char *fmt, ...);
extern void warnx(const char *fmt, ...);
#endif

#ifndef __GNUC__
#define __attribute__(x)
#endif
