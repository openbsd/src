/*	$OpenBSD: config.h,v 1.17 2018/01/05 15:44:31 jca Exp $	*/

/* config.h.  NOT generated automatically. */

/*
 * This file, config.h, which is a part of pdksh (the public domain ksh),
 * is placed in the public domain.  It comes with no licence, warranty
 * or guarantee of any kind (i.e., at your own risk).
 */

#ifndef CONFIG_H
#define CONFIG_H

/* Include brace-expansion? */
#define BRACE_EXPAND 1

/* Include any history? */
#define HISTORY 1

/* Strict POSIX behaviour? */
/* #undef POSIXLY_CORRECT */

/* Specify default $ENV? */
/* #undef DEFAULT_ENV */

/*
 * End of configuration stuff for PD ksh.
 */

#if defined(EMACS) || defined(VI)
# define	EDIT
#else
# undef		EDIT
#endif

/* Editing implies history */
#if defined(EDIT) && !defined(HISTORY)
# define HISTORY
#endif /* EDIT */

#endif /* CONFIG_H */
