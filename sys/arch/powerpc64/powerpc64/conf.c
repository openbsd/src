/*	$OpenBSD: conf.c,v 1.4 2020/06/27 21:22:30 kettenis Exp $	*/

/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *     @(#)conf.c	7.9 (Berkeley) 5/28/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include <machine/conf.h>

struct bdevsw bdevsw[] =
{
	bdev_notdef(),
};
int	nblkdev = nitems(bdevsw);

#define mmread	mmrw
#define mmwrite	mmrw
cdev_decl(mm);
#include "pty.h"
#include "opalcons.h"

struct cdevsw cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_ctty_init(1,ctty),		/* 1: controlling terminal */
	cdev_mm_init(1,mm),		/* 2: /dev/{null,mem,kmem,...} */
	cdev_notdef(),
	cdev_notdef(),
	cdev_tty_init(NPTY,pts),	/* 5: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 6: pseudo-tty master */
	cdev_ptm_init(NPTY,ptm),	/* XX: pseudo-tty ptm device */
	cdev_tty_init(NOPALCONS,opalcons), /* XX: OPAL console */
};
int	nchrdev = nitems(cdevsw);

dev_t	swapdev;

int
iskmemdev(dev_t dev)
{
	return 0;
}

int
iszerodev(dev_t dev)
{
	return 0;
}

dev_t
getnulldev(void)
{
	return makedev(0, 0);
}

int chrtoblktbl[] = {
	NODEV,
};
int nchrtoblktbl = nitems(chrtoblktbl);
