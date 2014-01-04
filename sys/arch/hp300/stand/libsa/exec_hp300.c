/*	$OpenBSD: exec_hp300.c,v 1.4 2014/01/04 10:49:21 miod Exp $	*/
/*	$NetBSD: exec.c,v 1.15 1996/10/13 02:29:01 christos Exp $	*/

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
 */

#include <sys/param.h>
#include <sys/reboot.h>

#include "stand.h"
#include "samachdep.h"

#include <lib/libsa/loadfile.h>

char   rnddata[BOOTRANDOM_MAX];		/* XXX dummy */

void
exec(char *path, void *loadaddr, int howto)
{
	u_long marks[MARK_MAX];
	int rc;

	marks[MARK_START] = (u_long)loadaddr;
	rc = loadfile(path, marks, LOAD_KERNEL | COUNT_KERNEL);
	if (rc != 0)
		return;

	printf("Start @ 0x%lx\n", marks[MARK_ENTRY]);

	machdep_start((char *)marks[MARK_ENTRY], howto, loadaddr,
	    (char *)marks[MARK_SYM], (char *)marks[MARK_END]);

	/* exec failed */
	printf("exec: kernel returned!\n");
	errno = ENOEXEC;
}
