/*	$NetBSD: crt0.c,v 1.1 1996/09/12 16:59:02 cgd Exp $	*/

/*
 * Copyright (c) 1995 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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

#ifdef ECOFF_COMPAT
#undef DYNAMIC
#endif

#include <stdlib.h>
#include <sys/syscall.h>
#ifdef DYNAMIC
#include "rtld.h"
#else
typedef void Obj_Entry;
#endif

/*
 * Lots of the chunks of this file cobbled together from pieces of
 * other NetBSD crt files, including the common code.
 */

extern int	__syscall __P((int, ...));
#define	_exit(v)	__syscall(SYS_exit, (v))
#define	write(fd, s, n)	__syscall(SYS_write, (fd), (s), (n))

#define _FATAL(str)				\
	do {					\
		write(2, str, sizeof(str));	\
		_exit(1);			\
	} while (0)

static char	*_strrchr __P((char *, char));


char	**environ;
char	*__progname = "";

#ifndef ECOFF_COMPAT
extern void	__init __P((void));
extern void	__fini __P((void));
#endif /* ECOFF_COMPAT */

#ifdef DYNAMIC
void		rtld_setup __P((void (*)(void), const Obj_Entry *obj));

const Obj_Entry *mainprog_obj;

/*
 * Arrange for _DYNAMIC to exist weakly at address zero.  That way,
 * if we happen to be compiling without -static but with without any
 * shared libs present, things will still work.
 */
asm(".weak _DYNAMIC; _DYNAMIC = 0");
extern int _DYNAMIC;
#endif /* DYNAMIC */

#ifdef MCRT0
extern void	monstartup __P((u_long, u_long));
extern void	_mcleanup __P((void));
extern unsigned char _etext, _eprol;
#endif /* MCRT0 */

void
__start(sp, cleanup, obj)
	char **sp;
	void (*cleanup) __P((void));		/* from shared loader */
	const Obj_Entry *obj;			/* from shared loader */
{
	long argc;
	char **argv, *namep;

	argc = *(long *)sp;
	argv = sp + 1;
	environ = sp + 2 + argc;		/* 2: argc + NULL ending argv */

	if ((namep = argv[0]) != NULL) {	/* NULL ptr if argc = 0 */
		if ((__progname = _strrchr(namep, '/')) == NULL)
			__progname = namep;
		else
			__progname++;
	}

#ifdef DYNAMIC
	if (&_DYNAMIC != NULL)
		rtld_setup(cleanup, obj);
#endif

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&_eprol, (u_long)&_etext);
#endif

#ifndef ECOFF_COMPAT
	atexit(__fini);
	__init();
#endif /* ECOFF_COMPAT */

	exit(main(argc, argv, environ));
}


static char *
_strrchr(p, ch)
register char *p, ch;
{
	register char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
/* NOTREACHED */
}

#ifdef MCRT0
asm ("  .text");
asm ("_eprol:");
#endif

#ifdef DYNAMIC
void
rtld_setup(cleanup, obj)
	void (*cleanup) __P((void));
	const Obj_Entry *obj;
{

	if ((obj == NULL) || (obj->magic != RTLD_MAGIC))
		_FATAL("Corrupt Obj_Entry pointer in GOT");
	if (obj->version != RTLD_VERSION)
		_FATAL("Dynamic linker version mismatch");

	mainprog_obj = obj;
	atexit(cleanup);
}

void *
dlopen(name, mode)
	char	*name;
	int	mode;
{

	if (mainprog_obj == NULL)
		return NULL;
	return (mainprog_obj->dlopen)(name, mode);
}

int
dlclose(fd)
	void	*fd;
{

	if (mainprog_obj == NULL)
		return -1;
	return (mainprog_obj->dlclose)(fd);
}

void *
dlsym(fd, name)
	void	*fd;
	char	*name;
{

	if (mainprog_obj == NULL)
		return NULL;
	return (mainprog_obj->dlsym)(fd, name);
}

#if 0 /* not supported for ELF shlibs, apparently */
int
dlctl(fd, cmd, arg)
	void *fd, *arg;
	int cmd;
{

	if (mainprog_obj == NULL)
		return -1;
	return (mainprog_obj->dlctl)(fd, cmd, arg);
}
#endif

char *
dlerror()
{

	if (mainprog_obj == NULL)
		return NULL;
	return (mainprog_obj->dlerror)();
}
#endif /* DYNAMIC */
