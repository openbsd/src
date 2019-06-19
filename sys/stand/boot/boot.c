/*	$OpenBSD: boot.c,v 1.48 2019/04/10 19:41:03 florian Exp $	*/

/*
 * Copyright (c) 2003 Dale Rahn
 * Copyright (c) 1997,1998 Michael Shalayeff
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

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <libsa.h>
#include <lib/libsa/loadfile.h>
#include <lib/libkern/funcs.h>

#include <stand/boot/bootarg.h>

#include "cmd.h"

#ifndef KERNEL
#define KERNEL "/bsd"
#endif

char prog_ident[40];
char *progname = "BOOT";

extern	const char version[];
struct cmd_state cmd;

/* bootprompt can be set by MD code to avoid prompt first time round */
int bootprompt = 1;
char *kernelfile = KERNEL;		/* can be changed by MD code */
int boottimeout = 5;			/* can be changed by MD code */

char	rnddata[BOOTRANDOM_MAX];

void
boot(dev_t bootdev)
{
	int fd;
	int try = 0, st;
	uint64_t marks[MARK_MAX];

	machdep();

	snprintf(prog_ident, sizeof(prog_ident),
	    ">> OpenBSD/" MACHINE " %s %s", progname, version);
	printf("%s\n", prog_ident);

	devboot(bootdev, cmd.bootdev);
	strlcpy(cmd.image, kernelfile, sizeof(cmd.image));
	cmd.boothowto = 0;
	cmd.conf = "/etc/boot.conf";
	cmd.addr = (void *)DEFAULT_KERNEL_ADDRESS;
	cmd.timeout = boottimeout;

	if (upgrade()) {
		strlcpy(cmd.image, "/bsd.upgrade", sizeof(cmd.image));
		printf("upgrade detected: switching to %s\n", cmd.image);
	}

	st = read_conf();

#ifdef HIBERNATE
	int bootdev_has_hibernate(void);

	if (bootdev_has_hibernate()) {
		strlcpy(cmd.image, "/bsd.booted", sizeof(cmd.image));
		printf("unhibernate detected: switching to %s\n", cmd.image);
	}
#endif

	if (!bootprompt)
		snprintf(cmd.path, sizeof cmd.path, "%s:%s",
		    cmd.bootdev, cmd.image);

	while (1) {
		/* no boot.conf, or no boot cmd in there */
		if (bootprompt && st <= 0) {
			do {
				printf("boot> ");
			} while(!getcmd());
		}

		loadrandom(BOOTRANDOM, rnddata, sizeof(rnddata));
#ifdef MDRANDOM
		mdrandom(rnddata, sizeof(rnddata));
#endif
#ifdef FWRANDOM
		fwrandom(rnddata, sizeof(rnddata));
#endif

		st = 0;
		bootprompt = 1;	/* allow reselect should we fail */

		printf("booting %s: ", cmd.path);
		marks[MARK_START] = (u_long)cmd.addr;
		if ((fd = loadfile(cmd.path, marks, LOAD_ALL)) != -1) {
			close(fd);
			break;
		}

		kernelfile = KERNEL;
		try++;
		strlcpy(cmd.image, kernelfile, sizeof(cmd.image));
		printf(" failed(%d). will try %s\n", errno, kernelfile);

		if (try < 2) {
			if (cmd.timeout > 0)
				cmd.timeout++;
		} else {
			if (cmd.timeout)
				printf("Turning timeout off.\n");
			cmd.timeout = 0;
		}
	}

	/* exec */
	run_loadfile(marks, cmd.boothowto);
}

void
loadrandom(char *name, char *buf, size_t buflen)
{
	char path[MAXPATHLEN];
	struct stat sb;
	int fd, i;

#define O_RDONLY	0

	/* Extract the device name from the kernel we are loading. */
	for (i = 0; i < sizeof(cmd.path); i++) {
		if (cmd.path[i] == ':') {
			strlcpy(path, cmd.path, i + 1);
			snprintf(path + i, sizeof(path) - i, ":%s", name);
			break;
		} else if (cmd.path[i] == '\0') {
			snprintf(path, sizeof path, "%s:%s",
			    cmd.bootdev, name);
			break;
		}
	}

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		if (errno != EPERM)
			printf("cannot open %s: %s\n", path, strerror(errno));
		return;
	}
	if (fstat(fd, &sb) == -1 ||
	    sb.st_uid != 0 ||
	    (sb.st_mode & (S_IWOTH|S_IROTH)))
		goto fail;
	(void) read(fd, buf, buflen);
fail:
	close(fd);
}
