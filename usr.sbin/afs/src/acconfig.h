@BOTTOM@

/* RCSID */
#define RCSID(msg) \
static /**/const char *const rcsid[] = { (char *)rcsid, "\100(#)" msg }

/* Maximum values on all known systems */
#define MaxHostNameLen (64+4)
#define MaxPathLen (1024+4)

/* we always have stds.h */
#define HAVE_STDS_H

/*
 * Defintions that are ugly but needed to get all the symbols used
 */

/*
 * Defining this enables lots of useful (and used) extensions on
 * glibc-based systems such as Linux
 */

#define _GNU_SOURCE

/*
 * Defining this enables us to get the definition of `sigset_t' and
 * other importatnt definitions on Solaris.
 */

#ifndef __EXTENSIONS__
#define __EXTENSIONS__
#endif

#ifndef HAVE___ATTRIBUTE__
#define __attribute__(x)
#endif
