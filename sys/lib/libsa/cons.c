/*	$OpenBSD: cons.c,v 1.6 1997/08/04 20:31:21 mickey Exp $	*/

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
#include <dev/cons.h>
#include "stand.h"

static const struct consw *console = &consw[0];
int consdev;

int
cons_probe()
{
	int i, f = 0;
	consdev = CN_NORMAL;
	for (i = 0; i < ncons; i++) {
		if ((consw[i].cn_probe)() != 0) {
			if (f == 0)
				f++, console = &consw[i];
			printf("%s present\n", consw[i].name);
		}
	}
	if (!f)	/* not found */
		printf("no any console detected, ");
	printf("using %s console\n", console->name);
	return 1;
}

char *
ttyname(fd)
	int fd;
{
	if (fd)
		return "(not a tty)";
	else
		return console->name;
}

void
putc(c)
	int	c;
{
	(*console->cn_putc)(c);
}

int
getc()
{
	return (*console->cn_getc)();
}

int
ischar()
{
	return (*console->cn_ischar)();
}

