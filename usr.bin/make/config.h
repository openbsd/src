#ifndef CONFIG_H
#define CONFIG_H

/*	$OpenPackages$ */
/*	$OpenBSD: config.h,v 1.16 2007/09/16 09:49:24 espie Exp $	*/
/*	$NetBSD: config.h,v 1.7 1996/11/06 17:59:03 christos Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 *	from: @(#)config.h	8.1 (Berkeley) 6/6/93
 */

#define DEFSHELL	1			/* Bourne shell */

/*
 * DEFMAXJOBS
 * DEFMAXLOCAL
 *	These control the default concurrency. On no occasion will more
 *	than DEFMAXJOBS targets be created at once (locally or remotely)
 *	DEFMAXLOCAL is the highest number of targets which will be
 *	created on the local machine at once. Note that if you set this
 *	to 0, nothing will ever happen...
 */
#define DEFMAXJOBS	4
#define DEFMAXLOCAL	1

/*
 * LIBSUFF
 *	Is the suffix used to denote libraries and is used by the Suff module
 *	to find the search path on which to seek any -l<xx> targets.
 */
#define LIBSUFF ".a"

/*
 * SYSVINCLUDE
 *	Recognize system V like include directives [include "filename"]
 * SYSVVARSUB
 *	Recognize system V like ${VAR:x=y} variable substitutions
 */
#define SYSVINCLUDE
#define SYSVVARSUB

/*
 * SUNSHCMD
 *	Recognize SunOS and Solaris:
 *		VAR :sh= CMD	# Assign VAR to the command substitution of CMD
 *		${VAR:sh}	# Return the command substitution of the value
 *				# of ${VAR}
 */
#define SUNSHCMD

#if !defined(__svr4__) && !defined(__SVR4) && !defined(__ELF__)
# ifndef RANLIBMAG
#  define RANLIBMAG "__.SYMDEF"
# endif
#endif

#ifdef HAS_EXTENDED_GETCWD
#define dogetcwd()	getcwd(NULL, 0)
#else
#define dogetcwd()	getcwd(emalloc(PATH_MAX), PATH_MAX)
#endif

#ifdef SYSVINCLUDE
#define DOFEATURE_SYSVINCLUDE	FEATURE_SYSVINCLUDE
#else
#define DOFEATURE_SYSVINCLUDE	0
#endif
#ifdef SYSVVARSUB
#define DOFEATURE_SYSVVARSUB	FEATURE_SYSVVARSUB
#else
#define DOFEATURE_SYSVVARSUB	0
#endif
#ifdef SUNSHCMD
#define DOFEATURE_SUNSHCMD	FEATURE_SUNSHCMD
#else
#define DOFEATURE_SUNSHCMD	0
#endif

#ifndef DEFAULT_FEATURES
#define DEFAULT_FEATURES	(FEATURE_UPPERLOWER | DOFEATURE_SYSVVARSUB | DOFEATURE_SYSVINCLUDE | DOFEATURE_SUNSHCMD | FEATURE_RECVARS)
#endif

#define FEATURES(x)	((DEFAULT_FEATURES & (x)) != 0)
#define FEATURE_ODE		1
#define FEATURE_UNIQ		2
#define FEATURE_SORT		4
#define FEATURE_UPPERLOWER	8
#define FEATURE_SYSVVARSUB	16
#define FEATURE_SYSVINCLUDE	32
#define FEATURE_SUNSHCMD	64
#define FEATURE_RECVARS		128
#define FEATURE_CONDINCLUDE	256
#define FEATURE_ASSIGN		512
#define FEATURE_EXECMOD		1024

/*
 * There are several places where expandable buffers are used (parse.c and
 * var.c). This constant is merely the starting point for those buffers. If
 * lines tend to be much shorter than this, it would be best to reduce BSIZE.
 * If longer, it should be increased. Reducing it will cause more copying to
 * be done for longer lines, but will save space for shorter ones. In any
 * case, it ought to be a power of two simply because most storage allocation
 * schemes allocate in powers of two.
 */
#define MAKE_BSIZE		256	/* starting size for expandable buffers */

#endif
