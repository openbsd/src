/*	$OpenBSD: exec_mvme.c,v 1.14 2008/03/31 22:14:43 miod Exp $	*/


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

/*ARGSUSED*/
void
exec_mvme(file, flag)
	char    *file;
	int     flag;
{
	u_long marks[MARK_MAX];
	int options;
	int rc;
	void (*entry)(int, int, int, int, int, int, int);
	int bootdev;
	struct mvmeprom_brdid *id;

	id = mvmeprom_brdid();
	options = LOAD_KERNEL | COUNT_KERNEL;
	if ((flag & RB_NOSYM) != 0)
		options &= ~(LOAD_SYM | COUNT_SYM);

	marks[MARK_START] = 0;
	rc = loadfile(file, marks, options);
	if (rc != 0)
		return;

	printf("Start @ 0x%lx\n", marks[MARK_START]);
	printf("Controller Address 0x%x\n", bugargs.ctrl_addr);
	if (flag & RB_HALT)
		_rtt();

	bootdev = (bugargs.ctrl_lun << 8) | (bugargs.dev_lun & 0xFF);
	entry = (void(*)(int, int, int, int, int, int, int))marks[MARK_START];
	(*entry)(flag, bugargs.ctrl_addr, marks[MARK_END], marks[MARK_SYM],
	    0, bootdev, id->model);

	printf("exec: kernel returned!\n");
	return;
}
