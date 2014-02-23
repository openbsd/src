/*	$OpenBSD: boot.c,v 1.4 2014/02/23 20:01:04 miod Exp $	*/
/*	$NetBSD: boot.c,v 1.3 2013/03/05 15:34:53 tsutsui Exp $	*/

/*
 * Copyright (c) 1992 OMRON Corporation.
 *
 * This code is derived from software contributed to Berkeley by
 * OMRON Corporation.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * OMRON Corporation.
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
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

/*
 * boot.c -- boot program
 * by A.Fujita, MAR-01-1992
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#define _KERNEL
#include <sys/fcntl.h>
#undef _KERNEL

#include <lib/libkern/libkern.h>
#include <luna88k/stand/boot/samachdep.h>
#include <luna88k/stand/boot/status.h>
#include <lib/libsa/loadfile.h>

int howto;

int	loadrandom(const char *, char *, size_t);
#if 0
static int get_boot_device(const char *, int *, int *, int *);
#endif

void (*cpu_boot)(uint32_t, uint32_t);
uint32_t cpu_bootarg1;
uint32_t cpu_bootarg2;

char rnddata[BOOTRANDOM_MAX];

#if 0
int
get_boot_device(const char *s, int *devp, int *unitp, int *partp)
{
	const char *p = s;
	int unit = 0, part = 0;

	while (*p != '(') {
		if (*p == '\0')
			goto error;
		p++;
	}

	p++;
	for (; *p != ',' && *p != ')'; p++) {
		if (*p == '\0')
			goto error;
		if (*p >= '0' && *p <= '9')
			unit = (unit * 10) + (*p - '0');
	}

	if (*p == ',')
		p++;
	for (; *p != ')'; p++) {
		if (*p == '\0')
			goto error;
		if (*p >= '0' && *p <= '9')
			part = (part * 10) + (*p - '0');
	}

	*devp  = 0;	/* XXX not yet */
	*unitp = unit;	/* XXX should pass SCSI ID, not logical unit number */
	*partp = part;

	return 0;

error:
	return -1;
}
#endif

int
boot(int argc, char *argv[])
{
	char *line, *lparen, *rparen;
	char rndpath[MAXPATHLEN];
	static int rnd_loaded = 0;

	if (argc < 2)
		line = default_file;
	else
		line = argv[1];

	printf("Booting %s\n", line);

	/*
	 * Try and load randomness from the boot device.
	 */
	if (rnd_loaded == 0) {
		lparen = strchr(line, '(');
		if (lparen != NULL)
			rparen = strchr(line, ')');
		else
			rparen = NULL;
		if (rparen != NULL &&
		    rparen + 1 - line < sizeof rndpath) {
			rparen++;
			memcpy(rndpath, line, rparen - line);
			rndpath[rparen - line] = '\0';
			strlcat(rndpath, BOOTRANDOM, sizeof rndpath);
		} else
			strlcpy(rndpath, BOOTRANDOM, sizeof rndpath);

		rnd_loaded = loadrandom(rndpath, rnddata, sizeof(rnddata));
	}

	return bootunix(line);
}

int
bootunix(char *line)
{
	int io;
#if 0
	int dev, unit, part;
#endif
	u_long marks[MARK_MAX];

#if 0
	if (get_boot_device(line, &dev, &unit, &part) != 0) {
		printf("Bad file name %s\n", line);
		return ST_ERROR;
	}
#endif

	/* Note marks[MARK_START] is passed as an load address offset */
	memset(marks, 0, sizeof(marks));

	io = loadfile(line, marks, LOAD_KERNEL);
	if (io >= 0) {
#ifdef DEBUG
		printf("entry = 0x%lx\n", marks[MARK_ENTRY]);
		printf("ssym  = 0x%lx\n", marks[MARK_SYM]);
		printf("esym  = 0x%lx\n", marks[MARK_END]);
#endif

		cpu_bootarg1 = BOOT_MAGIC;
		cpu_bootarg2 = marks[MARK_END];
		cpu_boot = (void (*)(uint32_t, uint32_t))marks[MARK_ENTRY];
		(*cpu_boot)(cpu_bootarg1, cpu_bootarg2);
	}
	printf("Booting kernel failed. (%s)\n", strerror(errno));

	return ST_ERROR;
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
