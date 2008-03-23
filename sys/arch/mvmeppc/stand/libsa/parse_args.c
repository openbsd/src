/*	$OpenBSD: parse_args.c,v 1.4 2008/03/23 17:05:41 deraadt Exp $ */

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

struct flags {
	char c;
	short bit;
} bf[] = {
	{ 'a', RB_ASKNAME }, /* ask root */
	{ 'b', RB_HALT },
	{ 'c', RB_CONFIG },
	{ 'd', RB_KDB },
	{ 'm', RB_MINIROOT },
	{ 's', RB_SINGLE },
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
