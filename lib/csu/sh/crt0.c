/*	$OpenBSD: crt0.c,v 1.3 2007/03/13 21:42:33 miod Exp $	*/
/*	$NetBSD: crt0.c,v 1.10 2004/08/26 21:16:41 thorpej Exp $ */

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
 *          NetBSD Project.  See http://www.NetBSD.org/ for
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

#include <sys/cdefs.h>
#include <sys/param.h>

#include <machine/asm.h>
#include <machine/fpu.h>
#include <stdlib.h>

static char     *_strrchr(const char *, char);

char    **environ;

char * __progname = "";

char __progname_storage[NAME_MAX+1];

#ifdef MCRT0
extern void     monstartup(u_long, u_long);
extern void     _mcleanup(void);
extern unsigned char _etext, _eprol;
#endif /* MCRT0 */

extern	void	_start(void);
void		___start(int, char *[], char *[], void *,
				const void *, void (*)(void));

__asm(	"	.text			\n"
	"	.align 2		\n"
	"	.globl _start		\n"
	"	.globl __start		\n"
	"_start:			\n"
	"__start:			\n"
	"	mov.l	r9,@-r15	\n"
	"	bra ___start		\n"
	"	 mov.l	r8,@-r15");

void
___start(int argc, char **argv, char **envp, void *ps_strings,
	const void *obj, void (*cleanup)(void))
{
	char *s;

#if defined(__SH4__) && !defined(__SH4_NOFPU__)
	extern void __set_fpscr(unsigned int);
	extern unsigned int __fpscr_values[2];

	__set_fpscr(0);
	__fpscr_values[0] |= FPSCR_DN;
	__fpscr_values[1] |= FPSCR_DN;
	__asm__ __volatile__ ("lds %0, fpscr" : : "r" (__fpscr_values[1]));
#endif

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
#endif	/* MCRT0 */

#ifndef SCRT0
        __init();
#endif

__asm("__callmain:");		/* Defined for the benefit of debuggers */
	exit(main(argc, argv, envp));
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
	/* NOTREACHED */
}
asm ("  .section \".text\" \n_eprol:");
