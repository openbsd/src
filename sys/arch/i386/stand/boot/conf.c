/*	$OpenBSD: conf.c,v 1.4 1997/07/17 23:11:19 mickey Exp $	*/

/*
 * Copyright (c) 1996 Michael Shalayeff
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
#include <netinet/in.h>
#include <libsa.h>
#include <lib/libsa/ufs.h>
#include <lib/libsa/nfs.h>
#include <lib/libsa/cd9660.h>
#ifdef notdef
#include <lib/libsa/fat.h>
#include <lib/libsa/tftp.h>
#endif
#include <lib/libsa/netif.h>
#include <lib/libsa/unixdev.h>
#include <biosdev.h>

const char version[] = "0.99a";
int	debug = 1;

struct fs_ops file_system[] = {
	{ ufs_open,    ufs_close,    ufs_read,    ufs_write,    ufs_seek,
	  ufs_stat,    ufs_readdir    },
#ifdef notdef
	{ fat_open,    fat_close,    fat_read,    fat_write,    fat_seek,
	  fat_stat,    fat_readdir    },
	{ cd9660_open, cd9660_close, cd9660_read, cd9660_write, cd9660_seek,
	  cd9660_stat, cd9660_readdir },
#endif
#ifndef NO_NET
	{ nfs_open,    nfs_close,    nfs_read,    nfs_write,    nfs_seek,
	  nfs_stat,    nfs_readdir    },
#endif
#ifdef _TEST
	{ null_open,   null_close,   null_read,   null_write,   null_seek,
	  null_stat,   null_readdir   }
#endif
};
int nfsys = NENTS(file_system);

struct devsw	devsw[] = {
#ifdef _TEST
	{ "UNIX", unixstrategy, unixopen, unixclose, unixioctl },
#else
	{ "BIOS", biosstrategy, biosopen, biosclose, biosioctl },
#endif
#if 0
	{ "TFTP", tftpstrategy, tftpopen, tftpclose, tftpioctl },
#endif
};
int ndevs = NENTS(devsw);

#ifndef NO_NET
struct netif_driver	*netif_drivers[] = {
	NULL
};
int n_netif_drivers = NENTS(netif_drivers);
#endif

const struct consw	consw[] = {
#ifdef _TEST
	{ "unix",unix_probe,unix_putc,unix_getc,unix_ischar},
#else
	{ "kbd",  kbd_probe,  kbd_putc,  kbd_getc,  kbd_ischar  },
	{ "com0", com0_probe, com0_putc, com0_getc, com0_ischar },
	{ "com1", com1_probe, com1_putc, com1_getc, com1_ischar },
	{ "com2", com2_probe, com2_putc, com2_getc, com2_ischar },
	{ "com3", com3_probe, com3_putc, com3_getc, com3_ischar },
#endif
};
const int ncons = NENTS(consw);

