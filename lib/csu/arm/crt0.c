/*	$OpenBSD: crt0.c,v 1.2 2004/02/01 05:44:37 drahn Exp $	*/
/*	$NetBSD: crt0.c,v 1.6 2002/01/01 01:31:06 thorpej Exp $	*/

/*
 * Copyright (C) 1997 Mark Brinicombe
 * Copyright (C) 1995 Wolfgang Solfrank.
 * Copyright (C) 1995 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>

#include <machine/asm.h>
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

__asm("	.text			\n"
"	.align	0		\n"
"	.globl	_start		\n"
"	.globl	__start		\n"
"_start:			\n"
"__start:			\n"
"	mov	r5, r2		/* cleanup */		\n"
"	mov	r4, r1		/* obj_main */		\n"
"	mov	r3, r0		/* ps_strings */	\n"
"	/* Get argc, argv, and envp from stack */	\n"
"	ldr	r0, [sp, #0x0000]	\n"
"	add	r1, sp, #0x0004		\n"
"	add	r2, r1, r0, lsl #2	\n"
"	add	r2, r2, #0x0004		\n"
"\n"
"	/* Ensure the stack is properly aligned before calling C code. */\n"
#if 1
"	bic	sp, sp, #7" /*__STRING(STACKALIGNBYTES)*/ "	\n"
#endif
"	sub	sp, sp, #8	\n"
"	str	r5, [sp, #4]	\n"
"	str	r4, [sp, #0]	\n"
"\n"
"	b	" __STRING(___start) " ");
/*
"	b	" __STRING(_C_LABEL(___start)) " ");
*/

void
___start(int argc, char **argv, char **envp, void *ps_strings,
	const void *obj, void (*cleanup)(void))
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

