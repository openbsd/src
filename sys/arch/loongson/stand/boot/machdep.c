/*	$OpenBSD: machdep.c,v 1.1 2010/02/14 22:39:33 miod Exp $	*/

/*
 * Copyright (c) 2010 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 1998-2004 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <lib/libkern/libkern.h>
#include "libsa.h"
#include <machine/cpu.h>
#include <machine/pmon.h>
#include <stand/boot/cmd.h>

int	pmon_quirks = 0;

/*
 * Console
 */

int
cnspeed(dev_t dev, int sp)
{
	return 9600;
}

int
getchar()
{
	int c = cngetc();

	if (c == '\r')
		c = '\n';

	if ((c < ' ' && c != '\n') || c == '\177')
		return c;

	putchar(c);

	return c;
}

int pch_pos;

void
putchar(int c)
{
	switch (c) {
	case '\177':	/* DEL erases */
		cnputc('\b');
		cnputc(' ');
	case '\b':
		cnputc('\b');
		if (pch_pos)
			pch_pos--;
		break;
	case '\t':
		do
			cnputc(' ');
		while (++pch_pos % 8) ;
		break;
	case '\n':
	case '\r':
		cnputc(c);
		pch_pos = 0;
		break;
	default:
		cnputc(c);
		pch_pos++;
		break;
	}
}

char *
ttyname(int fd)
{
	return "pmon console";
}

dev_t
ttydev(char *name)
{
	/* we do not support any other console than pmon */
	return NODEV;
}

/*
 * Configuration and device path aerobics
 */

/*
 * Return the default boot device.
 */
void
devboot(dev_t dev, char *path)
{
	const char *bootpath = NULL;
	size_t bootpathlen;
	const char *tmp;
	int i;

	/*
	 * First, try to figure where we have been loaded from; we'll assume
	 * the default device to load the kernel from is the same.
	 *
	 * We may have been loaded in three different ways:
	 * - automatic load from `al' environment variable.
	 * - manual `boot' command, with path on the commandline.
	 * - manual `load' and `go' commands, with no path on the commandline.
	 */

	if (pmon_argc > 0) {
		/* manual load */
		tmp = (const char *)pmon_getarg(0);
		if (tmp[0] != 'g') {
			for (i = 1; i < pmon_argc; i++) {
				tmp = (const char *)pmon_getarg(i);
				if (tmp[0] != '-') {
					bootpath = tmp;
					break;
				}
			}
		}
	} else {
		/* automatic load */
		bootpath = pmon_getenv("al");
	}

	/*
	 * If the bootblocks have been loaded from the network,
	 * use the default disk.
	 */

	if (bootpath != NULL && strncmp(bootpath, "tftp://", 7) == 0)
		bootpath = NULL;

	/*
	 * Now extract the device name from the bootpath.
	 */

	if (bootpath != NULL) {
		tmp = strchr(bootpath, '@');
		if (tmp == NULL) {
			bootpath = NULL;
		} else {
			bootpath = tmp + 1;
			tmp = strchr(bootpath, '/');
			if (tmp == NULL) {
				bootpath = NULL;
			} else {
				bootpathlen = tmp - bootpath;
			}
		}
	}
		
	if (bootpath != NULL) {
		if (bootpathlen >= BOOTDEVLEN)
			bootpathlen = BOOTDEVLEN - 1;
		strncpy(path, bootpath, bootpathlen);
		path[bootpathlen] = '\0';
	} else {
		tmp = pmon_getenv("Version");
		if (tmp != NULL && strncmp(tmp, "Gdium", 5) == 0)
			strlcpy(path, "usbg0", BOOTDEVLEN);
		else
			strlcpy(path, "wd0", BOOTDEVLEN);
	}
	strlcat(path, "a", BOOTDEVLEN);
}

/*
 * Ugly clock routines
 */

time_t
getsecs()
{
	return 0;
}

/*
 * Initialization
 */

void
machdep()
{
	const char *envvar;

	/*
	 * Figure out whether we are running on a Gdium system, which
	 * has an horribly castrated PMON.
	 */
	envvar = pmon_getenv("Version");
	if (envvar != NULL && strncmp(envvar, "Gdium", 5) == 0)
		pmon_quirks |= PQ_GDIUM;

	cninit();

	/*
	 * Since we can't have non-blocking input, we will try to
	 * autoload the kernel pointed to by the `bsd' environment
	 * variable, and fallback to interactive mode if the variable
	 * is empty or the load fails.
	 */

	envvar = pmon_getenv("bsd");
	if (envvar != NULL) {
		extern int bootprompt;
		extern char *kernelfile;

		bootprompt = 0;
		kernelfile = (char *)envvar;
	}
}

int
main()
{
	boot(0);
	return 0;
}
