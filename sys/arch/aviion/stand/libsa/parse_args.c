/*	$OpenBSD: parse_args.c,v 1.5 2013/10/10 21:22:07 miod Exp $ */

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

#include "stand.h"
#include "libsa.h"

#define KERNEL_NAME "bsd"

int	boothowto = 0;

/* skip end of token and whitespace */
static const char *stws(const char *);
static const char *
stws(const char *p)
{
	while (*p != ' ' && *p != '\0')
		p++;

	while (*p == ' ')
		p++;

	return p;
}

int
parse_args(const char *line, char **filep, int first)
{
	const char *po, *pc, *p;
	char *name;
	size_t namelen;

	if (first == 0) {
		/* recognize the special ``halt'' keyword */
		if (strcmp(line, "halt") == 0)
			return (1);
	}

	/* skip boot device; up to two nested foo(...) constructs */
	p = line;
	po = strchr(line, '(');
	if (po != NULL) {
		pc = strchr(po + 1, ')');
		if (pc != NULL) {
			p = strchr(po + 1, '(');
			if (p == NULL || pc < p)
				p = pc + 1;
			else if (pc != NULL) {
				p = strchr(pc + 1, ')');
				if (p != NULL)
					p = p + 1;
				else
					p = line;
			}
		}
	}

	/* skip partition number if any */
	pc = strchr(p, ':');
	if (pc != NULL)
		p = pc + 1;

	/* figure out how long the kernel name is */
	pc = strchr(p, ' ');
	if (pc == NULL)
		pc = p + strlen(p);

	if (p == pc) {
		/* empty, use the default kernel name */
		namelen = 1 + (p - line) + strlen(KERNEL_NAME);
		name = (char *)alloc(namelen);
		if (name == NULL)
			panic("out of memory");
		memcpy(name, line, p - line);
		memcpy(name + (p - line), KERNEL_NAME, sizeof(KERNEL_NAME));
	} else {
		namelen = pc - line;
		name = (char *)alloc(1 + namelen);
		if (name == NULL)
			panic("out of memory");
		memcpy(name, line, namelen);
		name[namelen] = '\0';
	}
	*filep = name;

	/*
	 * If this commandline is the one passed by the PROM, then look
	 * for options specific to the standalone code.
	 */

	if (first) {
		p = stws(p);
		while (*p != '\0') {
			if (*p++ == '-')
				while (*p != ' ' && *p != '\0')
					switch (*p++) {
					case 'z':
						boothowto |= BOOT_ETHERNET_ZERO;
						break;
					}
			p = stws(p);
		}
	}

	return 0;
}
