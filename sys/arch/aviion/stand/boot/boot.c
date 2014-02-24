/*	$OpenBSD: boot.c,v 1.5 2014/02/24 20:15:37 miod Exp $ */

/*-
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
#include <sys/stat.h>
#define _KERNEL
#include <sys/fcntl.h>
#undef _KERNEL

#include <machine/prom.h>

#include <lib/libkern/libkern.h>
#include "stand.h"
#include "libsa.h"

extern int devparse(const char *, uint *, uint *, uint *, uint *,
	    const char **, const char **, char **);
extern	const char version[];

char	line[80];
struct boot_info bi;

char   rnddata[BOOTRANDOM_MAX];		/* XXX dummy */

int	loadrandom(const char *, char *, size_t);

void
boot(const char *args, uint bootdev, uint bootunit, uint bootlun)
{
	char *p, *file, *fname;
	char rndpath[MAXPATHLEN];
	int ask;
	int ret;
	int rnd_loaded = 0;
	uint controller, unit, lun, part;
	const char *device, *ctrl;

	printf("\n>> OpenBSD/" MACHINE " boot %s\n", version);

	bi.bootdev = bootdev;
	bi.bootunit = bootunit;
	bi.bootlun = bootlun;
	bi.bootpart = 0;

	/*
	 * Older PROM version put a \r at the end of a manually entered
	 * boot string.
	 */
	if ((p = strchr(args, '\r')) != NULL)
		*p = '\0';

	ret = parse_args(args, &file, 1);
	ask = boothowto & RB_ASKNAME;
	for (;;) {
		if (ask != 0) {
			printf("boot: ");
			gets(line);
			if (line[0] == '\0')
				continue;

			ret = parse_args(line, &file, 0);
			args = line;
		}
		if (ret != 0) {
			printf("boot: returning to SCM\n");
			break;
		}

		/*
		 * Try and load randomness from the boot device.
		 */
		if (rnd_loaded == 0) {
			if (devparse(file, &controller, &unit, &lun, &part,
			    &device, &ctrl, &fname) == 0 &&
			    fname - file < sizeof(rndpath)) {
				memcpy(rndpath, file, fname - file);
				rndpath[fname - file] = '\0';
				strlcat(rndpath, BOOTRANDOM, sizeof rndpath);
				rnd_loaded = loadrandom(rndpath, rnddata,
				    sizeof(rnddata));
			}
		}

		printf("%s: ", file);
		exec(file, args,
		    bi.bootdev, bi.bootunit, bi.bootlun, bi.bootpart);
		printf("boot: %s: %s\n", file, strerror(errno));
		ask = 1;
	}
}

int
loadrandom(const char *name, char *buf, size_t buflen)
{
	struct stat sb;
	int fd;
	int rc = 0;

	fd = open(name, O_RDONLY);
	if (fd == -1) {
		if (errno != EPERM)
			printf("cannot open %s: %s\n", name, strerror(errno));
		return 0;
	}
	if (fstat(fd, &sb) == -1 || sb.st_uid != 0 || !S_ISREG(sb.st_mode) ||
	    (sb.st_mode & (S_IWOTH|S_IROTH)))
		goto fail;
	(void) read(fd, buf, buflen);
	rc = 1;
fail:
	close(fd);
	return rc;
}
