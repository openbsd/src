/*	$OpenBSD: crt0.c,v 1.2 2004/08/23 18:58:37 pvalchev Exp $	*/
/*	$NetBSD: crt0.c,v 1.7 1995/06/03 13:16:15 pk Exp $	*/
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
static char rcsid[] = "$OpenBSD: crt0.c,v 1.2 2004/08/23 18:58:37 pvalchev Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

extern void	__init __P((void));
extern void     _mcleanup __P((void));
extern unsigned char    eprol asm ("eprol");
extern unsigned char    etext;

char			**environ;
static char		empty[1];
char			*__progname = empty;
void     __perf_init __P((void));

static inline int
_perfcall(int func, void *arg)
{
  int status = 0;
#if 0
  __asm__ volatile (	"move   $4, %1\n\t"
			"move   $5, %2\n\t"
			"teqi   $0, 0\n\t"
			"move   %0, $2"
			: "=r" (status)
			: "r" (func), "r" (arg)
			: "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9", "$10",
			  "$11", "$12","$13","$14","$15","$24","$25", "memory");
#endif
  return(status);
}

void
__start()
{
	struct kframe {
		int	kargc;
		char	*kargv[1];	/* size depends on kargc */
		char	kargstr[1];	/* size varies */
		char	kenvstr[1];	/* size varies */
	};

	register struct kframe *kfp;
	register char **argv, *ap;

/*
 *  Do GP register setup. Differs depending on shared lib stuff or not.
 */
#if defined(_NO_ABICALLS)
	asm("	la	$28,_gp");
	asm("	addiu	%0,$29,32" : "=r" (kfp));
#else
	asm("	addiu	%0,$29,48" : "=r" (kfp));
#endif
	/* just above the saved frame pointer
	kfp = (struct kframe *) (&param-1);*/
	argv = &kfp->kargv[0];
	environ = argv + kfp->kargc + 1;

	if (ap = argv[0])
		if ((__progname = strrchr(ap, '/')) == NULL)
			__progname = ap;
		else
			++__progname;

asm("eprol:");

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&eprol, (u_long)&etext);
#endif /*MCRT0*/

	__init();
	__perf_init();

asm ("_callmain:");		/* Defined for the benefit of debuggers */
	exit(main(kfp->kargc, argv, environ));
}

#ifdef MCRT0
asm ("	.text");
asm ("_eprol:");
#endif

/************/
#include <stdio.h>

void     _perf_cleanup __P((void));

#define PCNT_CE                 0x0400  /* Count enable */
#define PCNT_UM                 0x0200  /* Count in User mode */
#define PCNT_KM                 0x0100  /* Count in kernel mode */

#define PCNT_FNC_SELECT         0x0001  /* Select counter source */
#define PCNT_FNC_READ           0x0002  /* Read current value of counter */

struct cname {
	char *name;
	int  cval;
} cn_tab[] = {
	{ "CLOCKS",	0x00 },
	{ "INSTR",	0x01 },
	{ "FPINSTR",	0x02 },
	{ "IINSTR",	0x03 },
	{ "LOAD",	0x04 },
	{ "STORE",	0x05 },
	{ "DUAL",	0x06 },
	{ "BRPREF",	0x07 },
	{ "EXTMISS",	0x08 },
	{ "STALL",	0x09 },
	{ "SECMISS",	0x0a },
	{ "INSMISS",	0x0b },
	{ "DTAMISS",	0x0c },
	{ "DTLBMISS",	0x0d },
	{ "ITLBMISS",	0x0e },
	{ "JTLBIMISS",	0x0f },
	{ "JTLBDMISS",	0x10 },
	{ "BRTAKEN",	0x11 },
	{ "BRISSUED",	0x12 },
	{ "SECWBACK",	0x13 },
	{ "PRIWBACK",	0x14 },
	{ "DCSTALL",	0x15 },
	{ "MISS",	0x16 },
	{ "FPEXC",	0x17 },
	{ "MULSLIP",	0x18 },
	{ "CP0SLIP",	0x19 },
	{ "LDSLIP",	0x1a },
	{ "WBFULL",	0x1b },
	{ "CISTALL",	0x1c },
	{ "MULSTALL",	0x1d },
	{ "ELDSTALL",	0x1e },
};
#define NCNTAB (sizeof(cn_tab) / sizeof(struct cname))

void
__perf_init()
{
	long pselect;
	char *cn;

	if((cn = getenv("__PERF_SELECT")) == NULL) {
		return;
	}
	
	for(pselect = 0; pselect < NCNTAB; pselect++) {
		if(strcmp(cn, cn_tab[pselect].name) == 0) {
			pselect = cn_tab[pselect].cval;
			break;
		}
	}
	if(pselect >= NCNTAB) {
		fprintf(stderr, "!! Invalid __PERF_SELECT !!\n");
		exit(255);
	}
	_perfcall(PCNT_FNC_SELECT, (void *)(PCNT_CE|PCNT_UM|pselect)); /*XXX*/
	atexit(_perf_cleanup);

	if((cn = getenv("__PERF_START")) == NULL) {
		return;
	}
}

void
_perf_cleanup()
{
	quad_t countvalue;

	_perfcall(PCNT_FNC_READ, &countvalue);
	printf("\ncount = %qd.\n", countvalue);
}
