/*	$OpenBSD: config.h,v 1.13 2004/12/18 22:42:26 millert Exp $	*/

/* config.h.  NOT generated automatically. */

/*
 * This file, config.h, which is a part of pdksh (the public domain ksh),
 * is placed in the public domain.  It comes with no licence, warranty
 * or guarantee of any kind (i.e., at your own risk).
 */

#ifndef CONFIG_H
#define CONFIG_H

/* Include emacs editing? */
#define EMACS 1

/* Include vi editing? */
#define VI 1

/* Include job control? */
#define JOBS 1

/* Include brace-expansion? */
#define BRACE_EXPAND 1

/* Include any history? */
#define HISTORY 1

/* Strict POSIX behaviour? */
/* #undef POSIXLY_CORRECT */

/* Specify default $ENV? */
/* #undef DEFAULT_ENV */

/* Include game-of-life? */
/* #undef SILLY */

/* The number of bytes in a int.  */
#define SIZEOF_INT 4

/*
 * End of configuration stuff for PD ksh.
 */

#if defined(EMACS) || defined(VI)
# define	EDIT
#else
# undef		EDIT
#endif

/* Super small configuration-- no editing. */
#if defined(EDIT) && defined(NOEDIT)
# undef EDIT
# undef EMACS
# undef VI
#endif

/* Editing implies history */
#if defined(EDIT) && !defined(HISTORY)
# define HISTORY
#endif /* EDIT */

#endif /* CONFIG_H */
