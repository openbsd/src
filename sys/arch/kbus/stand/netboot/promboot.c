/*	$OpenBSD: promboot.c,v 1.2 2000/03/03 00:54:50 todd Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed by Theo de Raadt
 * 4. The name of the Author may not be used to endorse or promote products
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
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include "stand.h"
#include "promboot.h"

char prom_bootdev[32];
char prom_bootfile[32];
int prom_boothow;
int debug;

void
prom_get_boot_info()
{
#if 0
	char	c, *src, *dst;
	extern int devlun, ctrlun;
	extern char *oparg, *opargend;

#ifdef	DEBUG
	printf("prom_get_boot_info\n");
#endif

	/* Get kernel filename */
	src = oparg;
	while (src && (*src == ' ' || *src == '\t'))
		src++;

	dst = prom_bootfile;
	if (src && *src != '-') {
		while (src && *src) {
			if (*src == ' ' || *src == '\t')
				break;
			*dst++ = *src++;
		}
	}
	*dst = '\0';

	/* Get boothowto flags */
	while (src && (*src == ' ' || *src == '\t'))
		src++;
	if (src && (*src == '-')) {
		while (*src) {
			switch (*src++) {
			case 'a':
				prom_boothow |= RB_ASKNAME;
				break;
			case 's':
				prom_boothow |= RB_SINGLE;
				break;
			case 'd':
				prom_boothow |= RB_KDB;
				debug = 1;
				break;
			}
		}
	}
#ifdef	DEBUG
	printf("promboot: device=\"%s\" file=\"%s\" how=0x%x\n",
		   prom_bootdev, prom_bootfile, prom_boothow);
#endif
#endif
}
