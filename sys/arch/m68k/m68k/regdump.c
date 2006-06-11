/*	$OpenBSD: regdump.c,v 1.6 2006/06/11 20:48:13 miod Exp $	*/
/*	$NetBSD: regdump.c,v 1.1 1997/04/09 19:21:47 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: Utah Hdr: machdep.c 1.74 92/12/20
 *	from: @(#)machdep.c	8.10 (Berkeley) 4/20/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/reg.h>
#include <machine/psl.h>

void dumpmem(int *, int, int);
char *hexstr(int, int);

/*
 * Print a register and stack dump.
 */
void
regdump(tf, sbytes)
	struct trapframe *tf; /* must not be register */
	int sbytes;
{
	static int doingdump = 0;
	register int i;
	int s;

	if (doingdump)
		return;
	s = splhigh();
	doingdump = 1;
	printf("pid = %d, pc = %s, ",
	    curproc ? curproc->p_pid : -1, hexstr(tf->tf_pc, 8));
	printf("ps = %s, ", hexstr(tf->tf_sr, 4));
	printf("sfc = %s, ", hexstr(getsfc(), 4));
	printf("dfc = %s\n", hexstr(getdfc(), 4));
	printf("Registers:\n     ");
	for (i = 0; i < 8; i++)
		printf("        %d", i);
	printf("\ndreg:");
	for (i = 0; i < 8; i++)
		printf(" %s", hexstr(tf->tf_regs[i], 8));
	printf("\nareg:");
	for (i = 0; i < 8; i++)
		printf(" %s", hexstr(tf->tf_regs[i+8], 8));
	if (sbytes > 0) {
		if (tf->tf_sr & PSL_S) {
			printf("\n\nKernel stack (%s):",
			    hexstr((int)(((int *)&tf)-1), 8));
			dumpmem(((int *)&tf)-1, sbytes, 0);
		} else {
			printf("\n\nUser stack (%s):",
			    hexstr(tf->tf_regs[SP], 8));
			dumpmem((int *)tf->tf_regs[SP], sbytes, 1);
		}
	}
	doingdump = 0;
	splx(s);
}

void
dumpmem(ptr, sz, ustack)
	int *ptr;
	int sz, ustack;
{
	int i, val;
	int limit;

	/* Stay in the same page */
	limit = ((int)ptr) | (NBPG-3);

	for (i = 0; i < sz; i++) {
		if ((i & 7) == 0)
			printf("\n%s: ", hexstr((int)ptr, 6));
		else
			printf(" ");
		if (ustack == 1) {
			if (copyin(ptr++, &val, sizeof(int)) != 0)
				break;
		} else {
			if (((int) ptr) >= limit)
				break;
			val = *ptr++;
		}
		printf("%s", hexstr(val, 8));
	}
	printf("\n");
}

char *
hexstr(val, len)
	register int val;
	int len;
{
	static char nbuf[9];
	register int x, i;

	if (len > 8)
		return("");
	nbuf[len] = '\0';
	for (i = len-1; i >= 0; --i) {
		x = val & 0xF;
		/* Isn't this a cool trick? */
		nbuf[i] = "0123456789ABCDEF"[x];
		val >>= 4;
	}
	return(nbuf);
}
