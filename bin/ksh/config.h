/*	$OpenBSD: config.h,v 1.11 2004/12/18 20:55:52 millert Exp $	*/

/* config.h.  NOT generated automatically. */

/*
 * This file, config.h, which is a part of pdksh (the public domain ksh),
 * is placed in the public domain.  It comes with no licence, warranty
 * or guarantee of any kind (i.e., at your own risk).
 */

#ifndef CONFIG_H
#define CONFIG_H




/* Define if C compiler groks __attribute__((...)) (const, noreturn, format) */
#define HAVE_GCC_FUNC_ATTR 1


/* Include ksh features? */
/* #define KSH 1 */

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

#ifdef HAVE_GCC_FUNC_ATTR
# define GCC_FUNC_ATTR(x)	__attribute__((x))
# define GCC_FUNC_ATTR2(x,y)	__attribute__((x,y))
#else
# define GCC_FUNC_ATTR(x)
# define GCC_FUNC_ATTR2(x,y)
#endif /* HAVE_GCC_FUNC_ATTR */

#endif /* CONFIG_H */
