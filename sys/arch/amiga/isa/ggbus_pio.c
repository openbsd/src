/*	$NetBSD: ggbus_pio.c,v 1.1 1995/08/04 14:32:17 niklas Exp $	*/

/*
 * Copyright (c) 1995 Niklas Hallqvist
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
 *      This product includes software developed by Niklas Hallqvist.
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
 */
#include <sys/types.h>
#include <sys/device.h>

#include <machine/pio.h>

#include <dev/isa/isavar.h>

#include <amiga/amiga/isr.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/isa/isa_machdep.h>
#include <amiga/isa/ggbusreg.h>
#include <amiga/isa/ggbusvar.h>

extern struct ggbus_device *ggbusp;

void		ggbus_outb __P((int, u_int8_t));
u_int8_t	ggbus_inb __P((int));
void		ggbus_outw __P((int, u_int16_t));
u_int16_t	ggbus_inw __P((int));

struct isa_pio_fcns ggbus_pio_fcns = {
	ggbus_inb,	isa_insb,
	ggbus_inw,	isa_insw,
	0 /* ggbus_inl */,	0 /* ggbus_insl */,
	ggbus_outb,	isa_outsb,
	ggbus_outw,	isa_outsw,
	0 /* ggbus_outl */,	0 /* ggbus_outsl */,
};

void
ggbus_outb(ia, b)
	int ia;
	u_int8_t b;
{
#ifdef DEBUG
	if (ggdebug)
		printf("outb 0x%x,0x%x\n", ia, b);
#endif
	*(volatile u_int8_t *)(ggbusp->gd_zargs.va + 2 * ia + 1) = b;
}

u_int8_t
ggbus_inb(ia)
	int ia;
{
	u_int8_t retval =
	    *(volatile u_int8_t *)(ggbusp->gd_zargs.va + 2 * ia + 1);


#ifdef DEBUG
	if (ggdebug)
		printf("inb 0x%x => 0x%x\n", ia, retval);
#endif
	return retval;
}

void
ggbus_outw(ia, w)
	int ia;
	u_int16_t w;
{
#ifdef DEBUG
	if (ggdebug)
		printf("outw 0x%x,0x%x\n", ia, w);
#endif
	*(volatile u_int16_t *)(ggbusp->gd_zargs.va + 2 * ia) = swap (w);
}

u_int16_t
ggbus_inw(ia)
	int ia;
{
	u_int16_t retval =
            swap(*(volatile u_int16_t *)(ggbusp->gd_zargs.va + 2 * ia));


#ifdef DEBUG
	if (ggdebug)
		printf("inw 0x%x => 0x%x\n", ia, retval);
#endif
	return retval;
}

