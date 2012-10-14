/*	$OpenBSD: ldomctl.c,v 1.1 2012/10/14 15:38:06 kettenis Exp $	*/

/*
 * Copyright (c) 2012 Mark Kettenis
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

#include <sys/types.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define HVCTL_RES_STATUS_DATA_SIZE	40

struct hvctl_header {
	uint16_t	op;
	uint16_t	seq;
	uint16_t	chksum;
	uint16_t	status;
};

struct hvctl_hello {
	uint16_t	major;
	uint16_t	minor;
};

struct hvctl_challenge {
	uint64_t	code;
};

struct hvctl_hvconfig {
	uint64_t	hv_membase;
	uint64_t	hv_memsize;
	uint64_t	hvmdp;
	uint64_t	del_reconf_hvmdp;
	uint32_t	del_reconf_gid;
};

struct hvctl_guest_op {
	uint32_t	guestid;
	uint32_t	code;
};

struct hvctl_res_status {
	uint32_t	res;
	uint32_t	resid;
	uint32_t	infoid;
	uint32_t	code;
	uint8_t         data[HVCTL_RES_STATUS_DATA_SIZE];
};

struct hvctl_msg {
	struct hvctl_header	hdr;
	union {
		struct hvctl_hello	hello;
		struct hvctl_challenge	clnge;
		struct hvctl_hvconfig	hvcnf;
		struct hvctl_guest_op	guestop;
		struct hvctl_res_status	resstat;
	} msg;
};

#define HVCTL_OP_GUEST_START	5
#define HVCTL_OP_GUEST_STOP	6

struct command {
	const char *cmd_name;
	void (*cmd_func)(int, char **);
};

__dead void usage(void);

void guest_start(int argc, char **argv);
void guest_stop(int argc, char **argv);

struct command commands[] = {
	{ "start",	guest_start },
	{ "stop",	guest_stop },
	{ NULL,		NULL }
};

int seq = 1;
int fd;

int
main(int argc, char **argv)
{
	struct command *cmdp;
	struct hvctl_msg msg;
	ssize_t nbytes;
	uint64_t code;

	if (argc != 3)
		usage();

	/* Skip program name. */
	argv++;
	argc--;

	for (cmdp = commands; cmdp->cmd_name != NULL; cmdp++)
		if (strcmp(argv[0], cmdp->cmd_name) == 0)
			break;
	if (cmdp->cmd_name == NULL)
		usage();

	fd = open("/dev/hvctl", O_RDWR, 0);
	if (fd == -1)
		err(1, "open");

	/*
	 * Say "Hello".
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.seq = seq++;
	msg.msg.hello.major = 1;
	nbytes = write(fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");

	code = msg.msg.clnge.code ^ 0xbadbeef20;

	/*
	 * Respond to challenge.
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = 2;
	msg.hdr.seq = seq++;
	msg.msg.clnge.code = code ^ 0x12cafe42a;
	nbytes = write(fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");

	(cmdp->cmd_func)(argc, argv);

	exit(EXIT_SUCCESS);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s start|stop domain\n", __progname);
	exit(EXIT_FAILURE);
}

void
guest_start(int argc, char **argv)
{
	struct hvctl_msg msg;
	ssize_t nbytes;

	/*
	 * Start guest domain.
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_GUEST_START;
	msg.hdr.seq = seq++;
	msg.msg.guestop.guestid = atoi(argv[1]);
	nbytes = write(fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");
}

void
guest_stop(int argc, char **argv)
{
	struct hvctl_msg msg;
	ssize_t nbytes;

	/*
	 * Stop guest domain.
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_GUEST_STOP;
	msg.hdr.seq = seq++;
	msg.msg.guestop.guestid = atoi(argv[1]);
	nbytes = write(fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");
}
