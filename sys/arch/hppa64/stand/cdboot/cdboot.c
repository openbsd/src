/*	$OpenBSD: cdboot.c,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <libsa.h>
#include <lib/libsa/cd9660.h>
#include <lib/libsa/loadfile.h>
#include <dev/cons.h>
#include <machine/pdc.h>
#include <stand/boot/bootarg.h>
#include "dev_hppa64.h"
#include "cmd.h"

dev_t bootdev;
int debug = 1;
int bootprompt = 1;

struct fs_ops file_system[] = {
	{ cd9660_open, cd9660_close, cd9660_read, cd9660_write, cd9660_seek,
	  cd9660_stat, cd9660_readdir },
};
int nfsys = NENTS(file_system);

struct devsw devsw[] = {
	{ "dk",	iodcstrategy, dkopen, dkclose, noioctl },
};
int	ndevs = NENTS(devsw);

struct consdev	constab[] = {
	{ ite_probe, ite_init, ite_getc, ite_putc },
	{ NULL }
};
struct consdev *cn_tab;

typedef void (*startfuncp)(int, int, int, int, int, int, caddr_t)
    __attribute__ ((noreturn));

void
boot(dev_t dev)
{
	u_long marks[MARK_MAX];
	char path[128];

	pdc_init();
	cninit();
	devboot(dev, path);
	strncpy(path + strlen(path), ":/bsd.rd", 9);
	printf(">> OpenBSD/" MACHINE " CDBOOT 0.1\n"
	    "booting %s: ", path);

	marks[MARK_START] = (u_long)DEFAULT_KERNEL_ADDRESS;
	if (!loadfile(path, marks, LOAD_KERNEL)) {
		marks[MARK_END] = ALIGN(marks[MARK_END] -
		    (u_long)DEFAULT_KERNEL_ADDRESS);
		fcacheall();

		__asm("mtctl %r0, %cr17");
		__asm("mtctl %r0, %cr17");
		(*(startfuncp)(marks[MARK_ENTRY]))((int)(long)pdc, 0, bootdev,
		    marks[MARK_END], BOOTARG_APIVER, BOOTARG_LEN,
		    (caddr_t)BOOTARG_OFF);
		/* not reached */
	}
}
