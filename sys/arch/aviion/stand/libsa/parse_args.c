/*	$OpenBSD: parse_args.c,v 1.2 2010/04/18 15:09:00 miod Exp $ */

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

int	boothowto = 0;

/* skip end of token and whitespace */
static char *stws(char *);
static char *
stws(char *p)
{
	while (*p != ' ' && *p != '\0')
		p++;

	while (*p == ' ')
		p++;

	return p;
}

int
parse_args(char *line, char **filep, int first)
{
	char *s = NULL, *p;
	char *name;
	size_t namelen;

	if (first == 0) {
		/* recognize the special ``halt'' keyword */
		if (strcmp(line, "halt") == 0)
			return (1);
	}

	/*
	 * The command line should be under the form
	 *   devtype(...)filename args
	 * such as
	 *   inen()bsd -s
	 * and we only care about the kernel name here.
	 *
	 * However, if the kernel could not be loaded, and we asked the
	 * user, he may not give the devtype() part - especially since
	 * at the moment we only support inen() anyway.
	 */

	/* search for a set of braces */
	for (p = line; *p != '\0' && *p != '('; p++) ;
	if (*p != '\0') {
		for (p = line; *p != '\0' && *p != ')'; p++) ;
		if (*p != '\0')
			s = ++p;
	}
		
	if (s == NULL)
		s = line;

	/* figure out how long the kernel name is */
	for (p = s; *p != '\0' && *p != ' '; p++) ;
	namelen = p - s;

	/* empty, use the default */
	if (namelen == 0)
		name = KERNEL_NAME;
	else {
		name = (char *)alloc(1 + namelen);
		if (name == NULL)
			panic("out of memory");
		bcopy(s, name, namelen);
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
