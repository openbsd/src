/*	$OpenBSD: crt0.c,v 1.8 2002/09/17 21:16:01 deraadt Exp $	*/
/*	$NetBSD: crt0.c,v 1.20 1995/06/03 13:16:08 pk Exp $	*/

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *
 */


#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: crt0.c,v 1.8 2002/09/17 21:16:01 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <stdlib.h>

#include "common.h"

extern void	start(void) asm("start");

void
start()
{
	struct kframe {
		int	kargc;
		char	*kargv[1];	/* size depends on kargc */
		char	kargstr[1];	/* size varies */
		char	kenvstr[1];	/* size varies */
	};
	/*
	 *	ALL REGISTER VARIABLES!!!
	 */
	register struct kframe *kfp;
	register char **argv, *ap;

#ifdef lint
	kfp = 0;
	initcode = initcode = 0;
#else /* not lint */
	/* just above the saved frame pointer */
	asm ("lea 4(%%ebp), %0" : "=r" (kfp) );
#endif /* not lint */
	argv = &kfp->kargv[0];
	environ = argv + kfp->kargc + 1;

	if ((ap = argv[0])) {	
		if ((__progname = _strrchr(ap, '/')) == NULL)
			__progname = ap;
		else
			++__progname;
	}

#ifdef DYNAMIC
	/* ld(1) convention: if DYNAMIC = 0 then statically linked */
#ifdef stupid_gcc
	if (&_DYNAMIC)
#else
	if ( ({volatile caddr_t x = (caddr_t)&_DYNAMIC; x; }) )
#endif
		__load_rtld(&_DYNAMIC);
#endif /* DYNAMIC */

asm("eprol:");

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&eprol, (u_long)&etext);
#endif /* MCRT0 */

asm ("__callmain:");		/* Defined for the benefit of debuggers */
	exit(main(kfp->kargc, argv, environ));
}

#ifdef DYNAMIC
	asm("	___syscall:");
	asm("		popl %ecx");
	asm("		popl %eax");
	asm("		pushl %ecx");
	asm("		int $0x80");
	asm("		pushl %ecx");
	asm("		jc 1f");
	asm("		ret");
	asm("	1:");
	asm("		movl $-1,%eax");
	asm("		ret");

#endif /* DYNAMIC */

#include "common.c"

#ifdef MCRT0
asm ("	.text");
asm ("_eprol:");
#endif
