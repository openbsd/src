/*	$OpenBSD: bioscons.c,v 1.21 2003/01/17 20:58:27 mickey Exp $	*/

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

#include <sys/types.h>
#include <machine/biosvar.h>
#include <machine/pio.h>
#include <dev/isa/isareg.h>
#include <dev/ic/mc146818reg.h>
#include <dev/ic/comreg.h>
#include <dev/ic/ns16450reg.h>
/* #include <i386/isa/nvram.h> */
#include <dev/cons.h>
#include <lib/libsa/stand.h>
#include "debug.h"

/* XXX cannot trust NVRAM on this.  Maybe later we make a real probe.  */
#if 0
#define PRESENT_MASK (NVRAM_EQUIPMENT_KBD|NVRAM_EQUIPMENT_DISPLAY)
#else
#define PRESENT_MASK 0
#endif

void
pc_probe(cn)
	struct consdev *cn;
{
	cn->cn_pri = CN_INTERNAL;
	cn->cn_dev = makedev(12, 0);
	printf(" pc%d", minor(cn->cn_dev));

#if 0
	outb(IO_RTC, NVRAM_EQUIPMENT);
	if ((inb(IO_RTC+1) & PRESENT_MASK) == PRESENT_MASK) {
		cn->cn_pri = CN_INTERNAL;
		/* XXX from i386/conf.c */
		cn->cn_dev = makedev(12, 0);
		printf(" pc%d", minor(cn->cn_dev));
	}
#endif
}

void
pc_init(cn)
	struct consdev *cn;
{
}

int
pc_getc(dev)
	dev_t dev;
{
	register int rv;

	if (dev & 0x80) {
		__asm __volatile(DOINT(0x16) "; setnz %b0" : "=a" (rv) :
		    "0" (0x100) : "%ecx", "%edx", "cc" );
		return (rv & 0xff);
	}

	__asm __volatile(DOINT(0x16) : "=a" (rv) : "0" (0x000) :
	    "%ecx", "%edx", "cc" );
	return (rv & 0xff);
}

void
pc_putc(dev, c)
	dev_t dev;
	int c;
{
	__asm __volatile(DOINT(0x10) : : "a" (c | 0xe00), "b" (1) :
	    "%ecx", "%edx", "cc" );
}

const int comports[4] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };

void
com_probe(cn)
	struct consdev *cn;
{
	register int i, n;

	/* get equip. (9-11 # of coms) */
	__asm __volatile(DOINT(0x11) : "=a" (n) : : "%ecx", "%edx", "cc");
	n >>= 9;
	n &= 7;
	for (i = 0; i < n; i++)
		printf(" com%d", i);
	if (n) {
		cn->cn_pri = CN_NORMAL;
		/* XXX from i386/conf.c */
		cn->cn_dev = makedev(8, 0);
	}
}

void
com_init(cn)
	struct consdev *cn;
{
	register int unit = minor(cn->cn_dev);

	/* let bios do necessary init first, 9600-N-1 */
	__asm __volatile(DOINT(0x14) : : "a" (0xe3), "d" (unit) :
	    "%ecx", "cc" );
}

int
com_getc(dev)
	dev_t dev;
{
	register int rv;

	if (dev & 0x80) {
		__asm __volatile(DOINT(0x14) : "=a" (rv) :
		    "0" (0x300), "d" (minor(dev&0x7f)) : "%ecx", "cc" );
		return ((rv & 0x100) == 0x100);
	}

	do
		__asm __volatile(DOINT(0x14) : "=a" (rv) :
		    "0" (0x200), "d" (minor(dev)) : "%ecx", "cc" );
	while (rv & 0x8000);

	return (rv & 0xff);
}

/* call with sp == 0 to query the current speed */
int com_speed = 9600;  /* default speed is 9600 baud */
int
comspeed(dev, sp)
	dev_t dev;
	int sp;
{
	int i, newsp;
        int err;

	if (sp <= 0)
		return com_speed;
	/* valid baud rate? */
	if (115200 < sp || sp < 75)
		return -1;

	for (i = sp; i != 75; i >>= 1)
		if (i & 1)
			return -1;

/* ripped screaming from dev/ic/com.c */
#define divrnd(n, q)    (((n)*2/(q)+1)/2)       /* divide and round off */
	newsp = divrnd((COM_FREQ / 16), sp);
	if (newsp <= 0)
		return -1;
	err = divrnd((COM_FREQ / 16) * 1000, sp * newsp) - 1000;
	if (err < 0)
		err = -err;
	if (err > COM_TOLERANCE)
		return -1;
#undef  divrnd(n, q)

	if (cn_tab && cn_tab->cn_dev == dev && com_speed != sp)
	{
		printf("com%d: changing speed to %d baud\n\a"
		       "com%d: change your terminal to match!\n\a"
		       "com%d: will change speed in 5 seconds....\n\a",
		       minor(dev), sp, minor(dev), minor(dev));
		sleep(5);
	}

	outb(comports[minor(dev)] + com_cfcr, LCR_DLAB);
	outb(comports[minor(dev)] + com_dlbl, newsp);
	outb(comports[minor(dev)] + com_dlbh, newsp>>8);
	outb(comports[minor(dev)] + com_cfcr, LCR_8BITS);
	printf("\ncom%d: console is at %d baud\n", minor(dev), sp);

	newsp = com_speed;
	com_speed = sp;
	return newsp;
}

void
com_putc(dev, c)
	dev_t dev;
	int c;
{
	register int rv;

	dev = minor(dev) & 0x7f;

	/* check online (DSR) */
	__asm __volatile(DOINT(0x14) : "=a" (rv) :
	    "0" (0x300), "d" (dev) : "%ecx", "cc" );
	if ( !(rv & 0x20) )
		return;

	/* send character */
	__asm __volatile(DOINT(0x14) : "=a" (rv) :
	    "0" (c | 0x100), "d" (dev) : "%ecx", "cc" );
}

