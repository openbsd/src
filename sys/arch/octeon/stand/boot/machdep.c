/*	$OpenBSD: machdep.c,v 1.5 2014/07/12 23:34:54 jasper Exp $	*/

/*
 * Copyright (c) 2009, 2010 Miodrag Vallat.
 * Copyright (c) 2013, 2014 Jasper Lievisse Adriaanse <jasper@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2003-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <lib/libkern/libkern.h>
#include "libsa.h"
#include <stand/boot/cmd.h>
#include <machine/cpu.h>
#include <machine/octeonreg.h>
#include <machine/octeonvar.h>

struct boot_desc *boot_desc;
struct boot_info *boot_info;

/*
 * We need to save the arguments u-boot setup for us, so we can pass them
 * onwards to the kernel later on.
 */
int
mips_init(__register_t a0, __register_t a1, __register_t a2 __unused,
	__register_t a3)
{
	boot_desc = (struct boot_desc *)a3;
	boot_info =
		(struct boot_info *)PHYS_TO_CKSEG0(boot_desc->boot_info_addr);

	boot(0);
	return 0;
}

/*
 * Console and TTY related functions
 */

int
cnspeed(dev_t dev, int s)
{
	return CONSPEED;
}

int pch_pos;

void
putchar(int c)
{
	switch (c) {
	case '\177':	/* DEL erases */
		cnputc('\b');
		cnputc(' ');
	case '\b':
		cnputc('\b');
		if (pch_pos)
			pch_pos--;
		break;
	case '\t':
		do
			cnputc(' ');
		while (++pch_pos % 8) ;
		break;
	case '\n':
	case '\r':
		cnputc(c);
		pch_pos = 0;
		break;
	default:
		cnputc(c);
		pch_pos++;
		break;
	}
}

char *
ttyname(int fd)
{
	return "uboot console";
}

dev_t
ttydev(char *name)
{
	return (NODEV);
}

/*
 * Boot -devices and -path related functions.
 */

void
devboot(dev_t dev, char *path)
{
	/* XXX:
	 * Assumes we booted from CF as this is currently the only supported
	 * disk. We may need to dig deeper to figure out the real root device.
	 */
	strlcpy(path, "octcf0a", BOOTDEVLEN);
}

time_t
getsecs(void)
{
	u_int ticks = cp0_get_count();
	uint32_t freq = boot_desc->eclock;

	return (time_t)((0xffffffff - ticks) / freq);
}

void
machdep()
{
	cninit();
}

__dead void
_rtt()
{
	octeon_xkphys_write_8(OCTEON_CIU_BASE + CIU_SOFT_RST, 1);
	for (;;) ;
}
