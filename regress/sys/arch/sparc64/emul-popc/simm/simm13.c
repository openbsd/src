/*	$OpenBSD: simm13.c,v 1.2 2003/08/19 05:37:57 jason Exp $	*/

/*
 * Copyright (c) 2003 Jason L. Wright (jason@thought.net)
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <machine/instr.h>
#include <err.h>
#include <stdio.h>

#define	SIGN_EXT13(v)	(((int64_t)(v) << 51) >> 51)

void
gen_simm(u_int32_t *p, int imm)
{
	/*
	 * generate the following asm, and flush the pipeline
	 *	retl
	 *	 popc imm, %o0
	 */
	p[0] = I_JMPLri(I_G0, I_O7, 8);
	__asm __volatile("iflush %0+0" : : "r" (&p[0]));
	p[1] = _I_OP3_R_RI(I_O0, IOP3_POPC, I_G0, imm);
	__asm __volatile("iflush %0+0" : : "r" (&p[1]));
	__asm __volatile("nop;nop;nop;nop;nop");
}

int
testit(void *v)
{
	int (*func)(void) = v;

	return ((*func)());
}

int64_t
c_popc(int64_t v)
{
	int64_t bit, r;

	for (bit = 1, r = 0; bit; bit <<= 1)
		if (v & bit)
			r++;
	return (r);
}

int
main()
{
	void *v;
	int i, a, c;
	int r = 0;

	v = mmap(NULL, 2 * sizeof(union instr), PROT_WRITE|PROT_EXEC|PROT_READ,
	    MAP_ANON, -1, 0);
	if (v == MAP_FAILED)
		err(1, "mmap");

	for (i = -4096; i <= 4095; i++) {
		gen_simm(v, i);
		c = c_popc(SIGN_EXT13(i));
		a = testit(v);
		if (c != a) {
			printf("BAD: %d: asm %d, c %d\n", i, a, c);
			r = 1;
		}
	}

	return (r);
}
