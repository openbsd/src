/*	$OpenBSD: crt0.c,v 1.10 2013/07/05 21:10:50 miod Exp $	*/
/*	$NetBSD: crt0.c,v 1.14 2002/05/16 19:38:21 wiz Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <limits.h>

#ifdef MCRT0
extern void monstartup(u_long, u_long);
extern void _mcleanup(void);
extern unsigned char _etext, _eprol;
#endif

char **environ;
char *__progname = "";
char __progname_storage[1 + NAME_MAX];

static char *_strrchr(const char *, char);

struct kframe {
	int	kargc;
	char	*kargv[1];	/* size depends on kargc */
	char	kargstr[1];	/* size varies */
	char	kenvstr[1];	/* size varies */
};

	asm("	.text");
	asm("	.align 2");
	asm("	.globl _start");
	asm("	.type _start,@function");
	asm("	_start:");
	asm("		.word 0x0101");		/* two nops just in case */
	asm("		pushl %sp");		/* no registers to save */
	asm("		calls $1,__start");	/* do the real start */
	asm("		halt");

void
__start(struct kframe *kfp)
{
	char **argv, *ap;
	char *s;

	argv = &kfp->kargv[0];
	environ = argv + kfp->kargc + 1;

	if ((__progname = argv[0]) != NULL) {
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
#endif /* MCRT0 */

	__init();

asm ("__callmain:");		/* Defined for the benefit of debuggers */
	exit(main(kfp->kargc, argv, environ));
}

static char *
_strrchr(const char *p, char ch)
{
	char *save;

	for (save = NULL; ; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (*p == '\0')
			return (save);
	}
}

#ifdef MCRT0
asm ("	.text");
asm ("_eprol:");
#endif
