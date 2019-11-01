/*	$OpenBSD: rdboot.c,v 1.3 2019/11/01 20:54:52 deraadt Exp $	*/

/*
 * Copyright (c) 2019 Visa Hankala
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/select.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>

#include <machine/octboot.h>
#include <machine/param.h>

#include "cmd.h"
#include "disk.h"

#define DEVRANDOM	"/dev/random"
#define BOOTRANDOM	"/etc/random.seed"
#define BOOTRANDOM_MAX	256	/* no point being greater than RC4STATE */
#define KERNEL		"/bsd"

void	loadrandom(void);
void	kexec(void);

struct cmd_state cmd;
int octbootfd = -1;
const char version[] = "1.1";

int
main(void)
{
	char rootdev[PATH_MAX];
	int fd, hasboot;

	fd = open(_PATH_CONSOLE, O_RDWR);
	login_tty(fd);

	/* Keep stdout unbuffered to mimic ordinary bootblocks. */
	setvbuf(stdout, NULL, _IONBF, 0);

	printf(">> OpenBSD/" MACHINE " BOOT %s\n", version);

	octbootfd = open("/dev/octboot", O_WRONLY);
	if (octbootfd == -1)
		err(1, "cannot open boot control device");

	memset(&cmd, 0, sizeof(cmd));
	cmd.boothowto = 0;
	cmd.conf = "/etc/boot.conf";
	strlcpy(cmd.image, KERNEL, sizeof(cmd.image));
	cmd.timeout = 5;

	if (ioctl(octbootfd, OBIOC_GETROOTDEV, rootdev) == -1) {
		if (errno != ENOENT)
			fprintf(stderr, "cannot get rootdev from kernel: %s\n",
			    strerror(errno));
	} else {
		snprintf(cmd.bootdev, sizeof(cmd.bootdev), "%s%sa",
		    rootdev, isduid(rootdev, OPENDEV_PART) ? "." : "");
	}

	disk_init();

	if (upgrade()) {
		strlcpy(cmd.image, "/bsd.upgrade", sizeof(cmd.image));
		printf("upgrade detected: switching to %s\n", cmd.image);
	}

	hasboot = read_conf();

	for (;;) {
		if (hasboot <= 0) {
			do {
				printf("boot> ");
			} while (!getcmd());
		}

		loadrandom();
		kexec();

		hasboot = 0;
		strlcpy(cmd.image, KERNEL, sizeof(cmd.image));
		printf("will try %s\n", cmd.image);
	}

	return 0;
}

void
loadrandom(void)
{
	char buf[BOOTRANDOM_MAX];
	int fd;

	/* Read the file from the device specified by the kernel path. */
	if (disk_open(cmd.path) == NULL)
		return;
	fd = open(BOOTRANDOM, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "%s: cannot open %s: %s", __func__, BOOTRANDOM,
		    strerror(errno));
		disk_close();
		return;
	}
	read(fd, buf, sizeof(buf));
	close(fd);
	disk_close();

	/*
	 * Push the whole buffer to the entropy pool.
	 * The kernel will use the entropy on kexec().
	 * It does not matter if some of the buffer content is uninitialized.
	 */
	fd = open(DEVRANDOM, O_WRONLY);
	if (fd == -1) {
		fprintf(stderr, "%s: cannot open %s: %s", __func__,
		    DEVRANDOM, strerror(errno));
		return;
	}
	write(fd, buf, sizeof(buf));
	close(fd);
}

void
kexec(void)
{
	struct octboot_kexec_args kargs;
	char kernelflags[8];
	char rootdev[32];
	const char *path;
	int argc, ret;

	path = disk_open(cmd.path);
	if (path == NULL)
		return;

	memset(&kargs, 0, sizeof(kargs));
	kargs.path = path;
	argc = 0;
	if (cmd.boothowto != 0) {
		snprintf(kernelflags, sizeof(kernelflags), "-%s%s%s%s",
		    (cmd.boothowto & RB_ASKNAME) ? "a" : "",
		    (cmd.boothowto & RB_CONFIG) ? "c" : "",
		    (cmd.boothowto & RB_KDB) ? "d" : "",
		    (cmd.boothowto & RB_SINGLE) ? "s" : "");
		kargs.argv[argc++] = kernelflags;
	}
	if (cmd.hasduid) {
		snprintf(rootdev, sizeof(rootdev),
		    "rootdev=%02x%02x%02x%02x%02x%02x%02x%02x",
		    cmd.bootduid[0], cmd.bootduid[1],
		    cmd.bootduid[2], cmd.bootduid[3],
		    cmd.bootduid[4], cmd.bootduid[5],
		    cmd.bootduid[6], cmd.bootduid[7]);
		kargs.argv[argc++] = rootdev;
	}

	printf("booting %s\n", cmd.path);
	ret = ioctl(octbootfd, OBIOC_KEXEC, &kargs);
	if (ret == -1)
		fprintf(stderr, "failed to execute kernel %s: %s\n",
		    cmd.path, strerror(errno));
	else
		fprintf(stderr, "kexec() returned unexpectedly\n");

	disk_close();
}
