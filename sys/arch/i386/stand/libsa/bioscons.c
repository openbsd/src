/*	$OpenBSD: bioscons.c,v 1.2 1997/08/12 23:34:21 mickey Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
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

#include <sys/types.h>
#include <machine/biosvar.h>
#include <machine/pio.h>
#include <dev/isa/isareg.h>
#include <dev/ic/mc146818reg.h>
#include <i386/isa/nvram.h>
#include <dev/cons.h>

#define PRESENT_MASK (NVRAM_EQUIPMENT_KBD|NVRAM_EQUIPMENT_DISPLAY)

void
kbd_probe(cn)
	struct consdev *cn;
{
	outb(IO_RTC, NVRAM_EQUIPMENT);
	if ((inb(IO_RTC+1) & PRESENT_MASK) == PRESENT_MASK) {
		cn->cn_pri = CN_INTERNAL;
		/* XXX from i386/conf.c */
		cn->cn_dev = makedev(12, 0);
	}
}

void
kbd_init(cn)
	struct consdev *cn;
{
	/* nothing */
}

int
kbd_getc(dev)
	dev_t dev;
{
	u_int8_t rv;

	if (dev & 0x80) {
		__asm volatile("movb $1, %%ah\n\t"
			       DOINT(0x16) "\n\t"
			       "setnz %%al"
			       : "=a" (rv) :: "%ecx", "%edx", "cc" );
		return rv;
	}

	__asm __volatile("xorl %%eax, %%eax\n\t"
			 DOINT(0x16)
			 : "=a" (rv) :: "%ecx", "edx", "cc" );
	return rv;
}

void
kbd_putc(dev, c)
	dev_t dev;
	int c;
{
	__asm __volatile("movb $0x0e, %%ah\n\t"
		       DOINT(0x10)
		       :: "a" (c), "b" (0) : "%ecx", "%edx", "cc" );
}

void
com_probe(cn)
	struct consdev *cn;
{
	register int i, n;
	__asm __volatile(DOINT(0x11) "\n\t" /* get equipment (9-11 # of coms) */
		       : "=a" (n) :: "%ecx", "%edx", "cc");
	n >>= 9;
	n &= 7;
	for (i = 0; i < n; i++)
		;
}

void
com_init(cn)
	struct consdev *cn;
{
	__asm volatile("movb $0xe2, %%al\n\t"
		       DOINT(0x14) "\n\t"
		       :: "d" (minor(cn->cn_dev)) : "%ecx", "cc" );
}

int
com_getc(dev)
	dev_t dev;
{
	register int rv;
	__asm volatile("movl $2, %%al\n\t"
		       DOINT(0x14) "\n\t"
		       : "=a" (rv): "d" (minor(dev)) : "%ecx", "cc" );

	if (!(rv & 0x8000))
		return 0;

	if (minor(dev) & 0x80)
		return 1;

	return (rv & 0xff);
}

void
com_putc(dev, c)
	dev_t dev;
	int c;
{
	int rv;
	__asm volatile("movb $1, %%ah\n\t"
		       DOINT(0x14) "\n\t"
		       : "=a" (rv): "d" (minor(dev)), "a" (c) : "%ecx", "cc" );
}
