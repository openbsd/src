/*	$OpenBSD: systrace-translate.c,v 1.3 2002/07/09 15:22:27 provos Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/tree.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <err.h>

#include "../../sys/compat/linux/linux_types.h"
#include "../../sys/compat/linux/linux_fcntl.h"

#include "intercept.h"
#include "systrace.h"

#define FL(w,c)	do { \
	if (flags & (w)) \
		*p++ = (c); \
} while (0)

int
print_oflags(char *buf, size_t buflen, struct intercept_translate *tl)
{
	char str[32], *p;
	int flags = (int)tl->trans_addr;
	int isread = 0;

	p = str;
	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		strcpy(p, "ro");
		isread = 1;
		break;
	case O_WRONLY:
		strcpy(p, "wo");
		break;
	case O_RDWR:
		strcpy(p, "rw");
		break;
	default:
		strcpy(p, "--");
		break;
	}

	/* XXX - Open handling of alias */
	if (isread)
		systrace_switch_alias("native", "open", "native", "fsread");
	else
		systrace_switch_alias("native", "open", "native", "fswrite");

	p += 2;

	FL(O_NONBLOCK, 'n');
	FL(O_APPEND, 'a');
	FL(O_CREAT, 'c');
	FL(O_TRUNC, 't');

	*p = '\0';

	strlcpy(buf, str, buflen);

	return (0);
}

int
linux_print_oflags(char *buf, size_t buflen, struct intercept_translate *tl)
{
	char str[32], *p;
	int flags = (int)tl->trans_addr;
	int isread = 0;

	p = str;
	switch (flags & LINUX_O_ACCMODE) {
	case LINUX_O_RDONLY:
		strcpy(p, "ro");
		isread = 1;
		break;
	case LINUX_O_WRONLY:
		strcpy(p, "wo");
		break;
	case LINUX_O_RDWR:
		strcpy(p, "rw");
		break;
	default:
		strcpy(p, "--");
		break;
	}

	/* XXX - Open handling of alias */
	if (isread)
		systrace_switch_alias("linux", "open", "linux", "fsread");
	else
		systrace_switch_alias("linux", "open", "linux", "fswrite");

	p += 2;

	FL(LINUX_O_APPEND, 'a');
	FL(LINUX_O_CREAT, 'c');
	FL(LINUX_O_TRUNC, 't');

	*p = '\0';

	strlcpy(buf, str, buflen);

	return (0);
}

int
print_modeflags(char *buf, size_t buflen, struct intercept_translate *tl)
{
	int mode = (int)tl->trans_addr;

	mode &= 00007777;
	snprintf(buf, buflen, "%o", mode);

	return (0);
}

int
print_number(char *buf, size_t buflen, struct intercept_translate *tl)
{
	int number = (int)tl->trans_addr;

	snprintf(buf, buflen, "%d", number);

	return (0);
}

struct intercept_translate oflags = {
	"oflags",
	NULL, print_oflags,
};

struct intercept_translate linux_oflags = {
	"oflags",
	NULL, linux_print_oflags,
};

struct intercept_translate modeflags = {
	"mode",
	NULL, print_modeflags,
};

struct intercept_translate uidt = {
	"uid",
	NULL, print_number,
};

struct intercept_translate gidt = {
	"gid",
	NULL, print_number,
};

struct intercept_translate fdt = {
	"fd",
	NULL, print_number,
};
