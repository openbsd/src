/* $NetBSD: crt0.c,v 1.9 2000/06/14 22:52:50 cgd Exp $ */

/*
 * Copyright (c) 1998 Christos Zoulas
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
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.netbsd.org/ for
 *          information about NetBSD.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * 
 * <<Id: LICENSE,v 1.2 2000/06/14 15:57:33 cgd Exp>>
 */

#include <stdlib.h>
#include <limits.h>

void ___start(int, char **, char **, void (*cleanup)(void), void *);

__asm(
"	.text\n"
"	.align 4\n"
"	.global	__start\n"
"	.global	_start\n"
"__start:\n"
"_start:\n"
"	mov	0, %fp\n"
"	ld	[%sp + 64], %o0		! get argc\n"
"	add	%sp, 68, %o1		! get argv\n"
"	sll	%o0, 2,	%o2\n"
"	add	%o2, 4,	%o2		! envp = argv + (argc << 2) + 4\n"
"	add	%o1, %o2, %o2\n"
"	andn	%sp, 7,	%sp		! align\n"
"	sub	%sp, 24, %sp		! expand to standard stack frame size\n"
"	mov	%g3, %o3\n"
"	mov	%g2, %o4\n"
"	call	___start\n"
"	 mov	%g1, %o5\n"
);

char **environ;
char *__progname = "";
char __progname_storage[NAME_MAX+1];

#ifdef MCRT0
extern void     monstartup(u_long, u_long);
extern void     _mcleanup(void);
extern unsigned char _etext, _eprol;
#endif /* MCRT0 */

static char *_strrchr(const char *, char);

void
___start(int argc, char **argv, char **envp, void (*cleanup)(void),
	void *obj)
{
	char *s;

	environ = envp;

	if ((__progname = argv[0]) != NULL) {	/* NULL ptr if argc = 0 */
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

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&_eprol, (u_long)&_etext);
#endif

	__init();

	exit(main(argc, argv, environ));
}

static char *
_strrchr(const char *p, char ch)
{
	char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
}

#ifdef MCRT0
asm ("  .text");
asm ("_eprol:");
#endif
