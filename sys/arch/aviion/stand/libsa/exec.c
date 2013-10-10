/*	$OpenBSD: exec.c,v 1.2 2013/10/10 21:22:07 miod Exp $	*/


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
 * 	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"

#include <lib/libsa/loadfile.h>

#define	BOOT_MAGIC	0x6274ef2e	/* need to match locore.S */

/*ARGSUSED*/
void
exec(char *file, const char *args, uint bootdev, uint bootunit, uint bootlun,
    uint bootpart)
{
	u_long marks[MARK_MAX];
	int rc;
	void (*entry)(const char *, uint, uint, uint, uint, uint, uint);

	marks[MARK_START] = 0;
	rc = loadfile(file, marks, LOAD_KERNEL | COUNT_KERNEL);
	if (rc != 0)
		return;

	entry = (void(*)(const char *, uint, uint, uint, uint, uint, uint))
	    marks[MARK_START];
	(*entry)(args, bootdev, bootunit, bootlun, BOOT_MAGIC + 1,
	    bootpart, marks[MARK_END]);

	printf("exec: kernel returned!\n");
}
