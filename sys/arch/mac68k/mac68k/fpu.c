/*	$OpenBSD: fpu.c,v 1.5 1996/10/14 01:15:52 briggs Exp $	*/
/*	$NetBSD: fpu.c,v 1.16 1996/06/11 02:56:22 scottr Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon Ross
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

/*
 * Floating Point Unit (MC68881/882/040)
 * Probe for the FPU at autoconfig time.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/psl.h>
#include <machine/cpu.h>
#include <machine/frame.h>

/*
 * FPU type; emulator uses FPU_NONE
 */
int     fputype;

extern int *nofault;

static int  fpu_match __P((struct device *, void *, void *));
static void fpu_attach __P((struct device *, struct device *, void *));
static int  fpu_probe __P((void));

struct cfattach fpu_ca = {
	sizeof(struct device), fpu_match, fpu_attach
};

struct cfdriver fpu_cd = {
	NULL, "fpu", DV_DULL, 0
};

static int
fpu_match(pdp, match, auxp)
	struct device	*pdp;
	void	*match, *auxp;
{
	return 1;
}

static char *fpu_descr[] = {
#ifdef	FPU_EMULATE
	"emulator", 		/* 0 */
#else
	"no math support",	/* 0 */
#endif
	"mc68881",			/* 1 */
	"mc68882",			/* 2 */
	"mc68040",			/* 3 */
	"mc68060",			/* 4 */
	"unknown" };

static void
fpu_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	char *descr;

	fputype = fpu_probe();
	if ((0 <= fputype) && (fputype <= 3))
		descr = fpu_descr[fputype];
	else
		descr = "unknown type";

	printf(" (%s)\n", descr);
}

static int
fpu_probe()
{
	/*
	 * A 68881 idle frame is 28 bytes and a 68882's is 60 bytes.
	 * We, of course, need to have enough room for either.
	 */
	int	fpframe[60 / sizeof(int)];
	label_t	faultbuf;
	u_char	b;

	nofault = (int *) &faultbuf;
	if (setjmp(&faultbuf)) {
		nofault = (int *) 0;
		return (FPU_NONE);
	}

	/*
	 * Synchronize FPU or cause a fault.
	 * This should leave the 881/882 in the IDLE state,
	 * state, so we can determine which we have by
	 * examining the size of the FP state frame
	 */
	asm("fnop");

	nofault = (int *) 0;

	/*
	 * Presumably, if we're an 040 and did not take exception
	 * above, we have an FPU.  Don't bother probing.
	 */
	if (mmutype == MMU_68040)
		return (FPU_68040);

	/*
	 * Presumably, this will not cause a fault--the fnop should
	 * have if this will.  We save the state in order to get the
	 * size of the frame.
	 */
	asm("movl %0, a0; fsave a0@" : : "a" (fpframe) : "a0" );

	b = *((u_char *) fpframe + 1);

	/*
	 * Now, restore a NULL state to reset the FPU.
	 */
	fpframe[0] = fpframe[1] = 0;
	m68881_restore((struct fpframe *) fpframe);

	/*
	 * The size of a 68881 IDLE frame is 0x18
	 *         and a 68882 frame is 0x38
	 */
	if (b == 0x18)
		return (FPU_68881);
	if (b == 0x38)
		return (FPU_68882);

	/*
	 * If it's not one of the above, we have no clue what it is.
	 */
	return (FPU_UNKNOWN);
}
