/*	$OpenBSD: machdep.c,v 1.28 1999/08/25 00:54:19 mickey Exp $	*/

/*
 * Copyright (c) 1997-1999 Michael Shalayeff
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "libsa.h"
#include <machine/apmvar.h>
#include <machine/biosvar.h>
#include "debug.h"

struct BIOS_regs	BIOS_regs;

#if defined(DEBUG) && !defined(_TEST)
#define CKPT(c)	(*(u_int16_t*)0xb8148 = 0x4700 + (c))
#else
#define CKPT(c) /* c */
#endif

extern int debug;

void
machdep()
{
	/* here */    CKPT('0');
#ifndef _TEST
	gateA20(1);   CKPT('1');
	debug_init(); CKPT('2');
#endif
	/* call console init before doing any io */
	printf("probing:");
	cninit();     CKPT('3');
#ifndef _TEST
	apmprobe();   CKPT('4');
	pciprobe();   CKPT('5');
/*	smpprobe();   CKPT('6'); */
	memprobe();   CKPT('7');
	printf("\n");

	diskprobe();  CKPT('8');
#endif
	CKPT('Z');
}
