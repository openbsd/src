/*	$OpenBSD: conf.c,v 1.1 2016/05/14 17:55:15 kettenis Exp $	*/

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
 */

#include <sys/param.h>
#include <sys/types.h>
#include <lib/libsa/stand.h>
#include <lib/libsa/ufs.h>
#include <dev/cons.h>

#include "efiboot.h"
#include "efidev.h"

const char version[] = "0.1";
int	debug = 0;

struct fs_ops file_system[] = {
	{ ufs_open,    ufs_close,    ufs_read,    ufs_write,    ufs_seek,
	  ufs_stat,    ufs_readdir    },
};
int nfsys = nitems(file_system);

struct devsw	devsw[] = {
	{ "efi", efistrategy, efiopen, eficlose, efiioctl },
};
int ndevs = nitems(devsw);

struct consdev constab[] = {
	{ efi_cons_probe, efi_cons_init, efi_cons_getc, efi_cons_putc },
	{ NULL }
};
struct consdev *cn_tab;
