/*	$OpenBSD: conf.c,v 1.6 2010/12/06 18:44:49 jasper Exp $	*/
/*	$NetBSD: conf.c,v 1.4 2005/12/11 12:17:06 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)conf.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>

#include <dev/cons.h>

#include "libsa.h"
#include <lib/libsa/ufs.h>

const char version[] = "1.0";
int	debug = 0;

/*
 * Device configuration
 */
struct devsw devsw[] = {
	{ "wd",	wdstrategy, wdopen, wdclose, noioctl },
};
int ndevs = nitems(devsw);

/*
 * Filesystem configuration
 */
struct fs_ops file_system[] = {
	{ ufs_open,    ufs_close,    ufs_read,    ufs_write,    ufs_seek,
	  ufs_stat,    ufs_readdir }
};
int nfsys = nitems(file_system);

/*
 * Console configuration
 */
struct consdev constab[] = {
	{ com_probe, com_init, com_getc, com_putc },
	{ NULL }
};
struct consdev *cn_tab;
