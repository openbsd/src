/*	$OpenBSD: exec_mvme.c,v 1.8 2004/01/29 21:30:04 miod Exp $	*/

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
#include <sys/exec_elf.h>

#include <machine/prom.h>

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>
#include <lib/libsa/loadfile.h>

#include "libsa.h"

/*ARGSUSED*/
void
exec_mvme(file, flag)
	char    *file;
	int     flag;
{
	int fd;
	u_long marks[MARK_MAX];
	void (*entry)();
	u_long *entryp;
	void *esym;
	int bootdev;

#ifdef DEBUG
	printf("exec_mvme: file=%s flag=0x%x cputyp=%x\n", file, flag, bugargs.cputyp);
#endif

	fd = open(file, 0);
	if (fd < 0)
		return;

	printf("Booting %s...", file);
	marks[MARK_START] = 0;
	if (loadfile(file, marks, LOAD_ALL) >= 0) {
		close(fd);

		entryp = (u_long *)&entry;
		*entryp = marks[MARK_ENTRY];
		esym = (void *)marks[MARK_END];

		printf("Start @ 0x%x\n", (unsigned int)entry);
		printf("Controller Address 0x%x\n", bugargs.ctrl_addr);
		if (flag & RB_HALT)
			mvmeprom_return();

		bootdev = (bugargs.ctrl_lun << 8) | (bugargs.dev_lun & 0xFF);
		/*
		 * Arguments to start on mvmeppc:
		 * r1 - stack provided by firmware/bootloader
		 * r3 - boot flags
		 * r4 - boot device
		 * r5 - firmware pointer (NULL for PPC1bug)
		 * r6 - arg list
		 * r7 - arg list length
		 * r8 - end of symbol table
		 */
		/*       r3                 r4       r5    r6    r7 r8 */
	 	(*entry)(flag, bootdev, NULL, NULL, 0, esym);

		printf("exec: kernel returned!\n");
	} else {
		close(fd);
	}
}
