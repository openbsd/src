/*	$OpenBSD: crt0.c,v 1.1 2003/02/26 18:50:35 drahn Exp $	*/
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
#include <sys/param.h>
#include <stdlib.h>

char **environ;
char * __progname = "";

char __progname_storage[MAXPATHLEN];

#ifdef MCRT0
extern void     monstartup(u_long, u_long);
extern void     _mcleanup(void);
extern unsigned char _etext, _eprol;
#endif /* MCRT0 */

static inline char * _strrchr(const char *p, char ch);

void
_start(int argc, char **argv, char **envp, void *aux, void (*cleanup)(void))
{
	char *s;

	environ = envp;

	if ((__progname = argv[0]) != NULL) {   /* NULL ptr if argc = 0 */
		if ((__progname = _strrchr(__progname, '/')) == NULL)
			__progname = argv[0];
		else
			__progname++;
		for (s = __progname_storage; *__progname &&
		   s < &__progname_storage[sizeof __progname_storage - 1]; )
			*s++ = *__progname++;
		*s = '\0';
		__progname = __progname_storage;
	}
#if 0
	atexit(cleanup);
#endif
#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&_eprol, (u_long)&_etext);
#endif

#ifndef SCRT0
	__init();
#endif

	exit(main(argc, argv, environ));
}

static inline char *
_strrchr(const char *p, char ch)
{
	char *save;
	
	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
/* NOTREACHED */
}
asm ("	.section \".text\" \n_eprol:");

