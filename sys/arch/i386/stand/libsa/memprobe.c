/*	$OpenBSD: memprobe.c,v 1.14 1997/10/12 21:01:01 mickey Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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
 *	This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <machine/biosvar.h>
#include "libsa.h"

static int addrprobe __P((u_int));
u_int cnvmem, extmem;

void
memprobe()
{
	int ram;

	__asm __volatile(DOINT(0x12) : "=a" (cnvmem)
			 :: "%ecx", "%edx", "cc");
	cnvmem &= 0xffff;

	printf("%d Kb conventional memory.\n", cnvmem);

	/* probe extended memory
	 *
	 * There is no need to do this in assembly language.  This is
	 * much easier to debug in C anyways.
	 */
	for(ram = 1024; ram < 512*1024; ram += 4){

		if(!(ram % 64))
			printf("Probing memory: %d KB\r", ram-1024);
		if(addrprobe(ram))
			break;
	}

	printf("%d Kb extended memory.\n", ram-1024);
	extmem = ram - 1024;
}

/* addrprobe(kloc): Probe memory at address kloc * 1024.
 *
 * This is a hack, but it seems to work ok.  Maybe this is
 * the *real* way that you are supposed to do probing???
 *
 * Modify the original a bit.  We write everything first, and
 * then test for the values.  This should croak on machines that
 * return values just written on non-existent memory...
 *
 * BTW: These machines are pretty broken IMHO.
 */
static int
addrprobe(kloc)
	u_int kloc;
{
	__volatile u_int *loc;
	static const u_int pat[] = {
		0x00000000, 0xFFFFFFFF,
		0x01010101, 0x10101010,
		0x55555555, 0xCCCCCCCC
	};
	register u_int i, ret = 0;
	u_int save[NENTS(pat)];

	/* Get location */
	loc = (int *)(kloc * 1024);

	save[0] = *loc;
	/* Probe address */
	for(i = 0; i < NENTS(pat); i++){
		*loc = pat[i];
		if(*loc != pat[i])
			ret++;
	}
	*loc = save[0];

	if (!ret) {
		/* Write address */
		for(i = 0; i < NENTS(pat); i++) {
			save[i] = loc[i];
			loc[i] = pat[i];
		}

		/* Read address */
		for(i = 0; i < NENTS(pat); i++) {
			if(loc[i] != pat[i])
				ret++;
			loc[i] = save[i];
		}
	}

	return ret;
}


