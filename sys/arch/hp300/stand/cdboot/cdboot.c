/*	$OpenBSD: cdboot.c,v 1.8 2011/08/18 20:02:58 miod Exp $	*/
/*	$NetBSD: uboot.c,v 1.3 1997/04/27 21:17:13 thorpej Exp $	*/

/*-
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
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <machine/exec.h>
#include <a.out.h>

#include <lib/libsa/stand.h>

#include "samachdep.h"

/*
 * Boot program... bits in `howto' determine whether boot stops to
 * ask for system name.	 Boot device is derived from ROM provided
 * information.
 */

extern	u_int opendev;
extern	char *lowram;
extern	int noconsole;

extern	const char version[];

/*
 * XXX UFS accepts a /, NFS doesn't.
 */
char *names[] = {
#ifdef OSREV
	OSREV "/hp300/bsd.rd",
#endif
	"bsd.rd", "bsd",
};
#define NUMNAMES	(sizeof(names) / sizeof(char *))

#if 0
static int bdev, badapt, bctlr, bunit, bpart;
#endif

int
main(void)
{
	int currname = 0;
	char *name;

	printf("\n>> OpenBSD [%dKB] CDROM BOOT %s HP 9000/%s CPU\n",
	       (__LDPGSZ / 1024), version, getmachineid());

#if 0
	bdev   = B_TYPE(bootdev);
	badapt = B_ADAPTOR(bootdev);
	bctlr  = B_CONTROLLER(bootdev);
	bunit  = B_UNIT(bootdev);
	bpart  = B_PARTITION(bootdev);
#endif

	for (;;) {
		name = names[currname++];
		if (currname == NUMNAMES)
			currname = 0;

		howto = RB_SINGLE;

		printf(": %s\n", name);

		exec(name, lowram, howto);
		printf("boot: %s\n", strerror(errno));
	}
	return (0);
}
