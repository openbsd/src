/*	$OpenBSD: parse_args.c,v 1.2 1999/09/27 19:30:01 smurph Exp $ */

/*-
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <machine/prom.h>
#include <a.out.h>

#include "stand.h"
#include "libsa.h"

#define KERNEL_NAME "bsd"
#define RB_NOSYM 0x400

#define	RB_AUTOBOOT	0	/* flags for system auto-booting itself */

#if 0
#define	RB_ASKNAME	0x0001	/* ask for file name to reboot from */
#define	RB_SINGLE	0x0002	/* reboot to single user only */
#define	RB_NOSYNC	0x0004	/* dont sync before reboot */
#define	RB_HALT		0x0008	/* don't reboot, just halt */
#define	RB_INITNAME	0x0010	/* name given for /etc/init (unused) */
#define	RB_DFLTROOT	0x0020	/* use compiled-in rootdev */
#define	RB_KDB		0x0040	/* give control to kernel debugger */
#define	RB_RDONLY	0x0080	/* mount root fs read-only */
#define	RB_DUMP		0x0100	/* dump kernel memory before reboot */
#define	RB_MINIROOT	0x0200	/* mini-root present in memory at boot time */
#define	RB_CONFIG	0x0400	/* change configured devices */
#define	RB_TIMEBAD	0x0800	/* don't call resettodr() in boot() */
#define	RB_POWERDOWN	0x1000	/* attempt to power down machine */
#define	RB_SERCONS	0x2000	/* use serial console if available */
#endif 

struct flags {
	char c;
	short bit;
} bf[] = {
	{ 'a', RB_ASKNAME }, /* ask root */
	{ 'b', RB_HALT },
	{ 'c', RB_CONFIG },
	{ 'd', RB_KDB },
	{ 'e', 0x4000 },  /* spin slave cpus  */
	{ 'f', 0x0010 },  /* ask kernel name  */
	{ 'm', RB_MINIROOT },
	{ 'r', RB_DFLTROOT },
	{ 's', RB_SINGLE },
	{ 'x', 0x8000 },  /* extra boot debug */
	{ 'y', RB_NOSYM },
};

int
parse_args(filep, flagp)

char **filep;
int *flagp;

{
	char *name = KERNEL_NAME, *ptr;
	int i, howto = 0;
	char c;

	if (bugargs.arg_start != bugargs.arg_end) {
		ptr = bugargs.arg_start;
		while (c = *ptr) {
			while (c == ' ')
				c = *++ptr;
			if (c == '\0')
				return (0);
			if (c != '-') {
				name = ptr;
				while ((c = *++ptr) && c != ' ')
					;
				if (c)
					*ptr++ = 0;
				continue;
			}
			while ((c = *++ptr) && c != ' ') {
				if (c == 'q')
					return (1);
				for (i = 0; i < sizeof(bf)/sizeof(bf[0]); i++)
					if (bf[i].c == c) {
						howto |= bf[i].bit;
					}
			}
		}
	}
	*flagp = howto;
	*filep = name;
	return (0);
}
