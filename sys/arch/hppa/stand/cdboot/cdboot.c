/*	$OpenBSD: cdboot.c,v 1.7 2004/06/14 00:32:31 deraadt Exp $	*/

/*
 * Copyright (c) 2003 Michael Shalayeff
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

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <libsa.h>
#include <lib/libsa/cd9660.h>
#include <lib/libsa/loadfile.h>
#include <dev/cons.h>
#include <machine/pdc.h>
#include <stand/boot/bootarg.h>
#include "dev_hppa.h"
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
		(*(startfuncp)(marks[MARK_ENTRY]))((int)pdc, 0, bootdev,
		    marks[MARK_END], BOOTARG_APIVER, BOOTARG_LEN,
		    (caddr_t)BOOTARG_OFF);
		/* not reached */
	}
}
