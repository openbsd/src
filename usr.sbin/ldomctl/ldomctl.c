/*	$OpenBSD: ldomctl.c,v 1.2 2012/10/14 16:11:45 kettenis Exp $	*/

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

#define SIS_NORMAL		0x1
#define SIS_TRANSITION		0x2
#define SOFT_STATE_SIZE		32

#define GUEST_STATE_STOPPED		0x0
#define GUEST_STATE_RESETTING		0x1
#define GUEST_STATE_NORMAL		0x2
#define GUEST_STATE_SUSPENDED		0x3
#define GUEST_STATE_EXITING		0x4
#define GUEST_STATE_UNCONFIGURED	0xff

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

struct hvctl_rs_guest_state {
	uint64_t	state;
};

struct hvctl_rs_guest_softstate {
	uint8_t		soft_state;
	char		soft_state_str[SOFT_STATE_SIZE];
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
#define HVCTL_OP_GET_RES_STAT	11

#define HVCTL_RES_GUEST		0

#define HVCTL_INFO_GUEST_STATE		0
#define HVCTL_INFO_GUEST_SOFT_STATE	1

struct command {
	const char *cmd_name;
	void (*cmd_func)(int, char **);
};

__dead void usage(void);

void guest_start(int argc, char **argv);
void guest_stop(int argc, char **argv);
void guest_status(int argc, char **argv);

struct command commands[] = {
	{ "start",	guest_start },
	{ "stop",	guest_stop },
	{ "status",	guest_status },
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

	fprintf(stderr, "usage: %s start|stop|status domain\n", __progname);
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

void
guest_status(int argc, char **argv)
{
	struct hvctl_msg msg;
	ssize_t nbytes;
	struct hvctl_rs_guest_state *state;
	struct hvctl_rs_guest_softstate *softstate;

	/*
	 * Request status.
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_GET_RES_STAT;
	msg.hdr.seq = seq++;
	msg.msg.resstat.res = HVCTL_RES_GUEST;
	msg.msg.resstat.resid = atoi(argv[1]);
	msg.msg.resstat.infoid = HVCTL_INFO_GUEST_STATE;
	nbytes = write(fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");

	state = (void *)msg.msg.resstat.data;
	switch (state->state) {
	case GUEST_STATE_STOPPED:
		printf("Stopped\n");
		return;
	case GUEST_STATE_NORMAL:
		break;
	case GUEST_STATE_UNCONFIGURED:
		printf("Unconfigured\n");
		return;
	default:
		printf("Unknown (%lld)\n", state->state);
		return;
	}

	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_GET_RES_STAT;
	msg.hdr.seq = seq++;
	msg.msg.resstat.res = HVCTL_RES_GUEST;
	msg.msg.resstat.resid = atoi(argv[1]);
	msg.msg.resstat.infoid = HVCTL_INFO_GUEST_SOFT_STATE;
	nbytes = write(fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");

	softstate = (void *)msg.msg.resstat.data;
	printf("%s\n", softstate->soft_state_str);
}
