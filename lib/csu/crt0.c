/*	$OpenBSD: crt0.c,v 1.3 2014/12/22 03:51:08 kurt Exp $	*/

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

#include <stdlib.h>
#include <limits.h>

#include "md_init.h"
#include "boot.h"

/* some defaults */
#ifndef	MD_START_ARGS
#define	MD_START_ARGS	\
	int argc, char **argv, char **envp, void (*cleanup)(void)
#endif
#ifndef MD_START
#define	MD_START	___start
static void		___start(MD_START_ARGS) __used;
#endif
#ifndef	MD_EPROL_LABEL
#define	MD_EPROL_LABEL	__asm("  .text\n_eprol:")
#endif

void	__init_tcb(char **_envp);
#pragma weak __init_tcb

static char	*_strrchr(char *, char);

char	**environ;
char	*__progname = "";
char	__progname_storage[NAME_MAX+1];

#ifdef MCRT0
extern void	monstartup(u_long, u_long);
extern void	_mcleanup(void);
extern unsigned char _etext, _eprol;
#endif /* MCRT0 */

#ifdef RCRT0
#ifdef MD_RCRT0_START
MD_RCRT0_START;
#endif
#else
#ifdef MD_CRT0_START
MD_CRT0_START;
#endif
#endif

void
MD_START(MD_START_ARGS)
{
	char *namep, *s;
#ifdef MD_START_SETUP
	MD_START_SETUP
#endif

	environ = envp;

	if ((namep = argv[0]) != NULL) {	/* NULL ptr if argc = 0 */
		if ((__progname = _strrchr(namep, '/')) == NULL)
			__progname = namep;
		else
			__progname++;
		for (s = __progname_storage; *__progname &&
		    s < &__progname_storage[sizeof __progname_storage - 1]; )
			*s++ = *__progname++;
		*s = '\0';
		__progname = __progname_storage;
	}

#ifndef MD_NO_CLEANUP
	if (cleanup != NULL)
		atexit(cleanup);
	else
#endif
	if (__init_tcb != NULL)
		__init_tcb(envp);

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&_eprol, (u_long)&_etext);
#endif

	__init();

	exit(main(argc, argv, envp));
}

static char *
_strrchr(char *p, char ch)
{
	char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = p;
		if (*p == '\0')
			return (save);
	}
}

#ifdef MCRT0
MD_EPROL_LABEL;
#endif
