/*	$OpenBSD: dlfcn.h,v 1.10 2007/05/05 15:21:21 drahn Exp $	*/
/*	$NetBSD: dlfcn.h,v 1.2 1995/06/05 19:38:00 pk Exp $	*/

/*
 * Copyright (c) 1995 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DLFCN_H_
#define _DLFCN_H_

#include <sys/cdefs.h>

/*
 * Structure filled in by dladdr().
 */
typedef	struct dl_info {
	const char	*dli_fname;	/* Pathname of shared object. */
	void		*dli_fbase;	/* Base address of shared object. */
	const char	*dli_sname;	/* Name of nearest symbol. */
	void		*dli_saddr;	/* Address of nearest symbol. */
} Dl_info;

/*
 * User interface to the run-time linker.
 */
__BEGIN_DECLS
extern void	*dlopen(const char *, int);
extern int	dlclose(void *);
extern void	*dlsym(void *, const char *);
extern int	dlctl(void *, int, void *);
extern const char	*dlerror(void);
extern int	dladdr(const void *, Dl_info *);
__END_DECLS

/* Values for dlopen `mode'. */
#define RTLD_LAZY	1
#define RTLD_NOW	2
#define RTLD_GLOBAL	0x100
#define RTLD_LOCAL	0x000
#define	DL_LAZY		RTLD_LAZY	/* Compat */

/*
 * Special handle arguments for dlsym().
 */
#define	RTLD_NEXT	((void *) -1)	/* Search subsequent objects. */
#define	RTLD_DEFAULT	((void *) -2)	/* Use default search algorithm. */
#define	RTLD_SELF	((void *) -3)	/* Search the caller itself. */

/*
 * dlctl() commands
 */
#define DL_GETERRNO	1
#define DL_SETSRCHPATH	x
#define DL_GETLIST	x
#define DL_GETREFCNT	x
#define DL_GETLOADADDR	x
#define DL_SETTHREADLCK	2
#define DL_SETBINDLCK	3

#endif /* _DLFCN_H_ */
