/* $OpenBSD: syslogc.c,v 1.5 2004/04/13 01:10:05 djm Exp $ */

/*
 * Copyright (c) 2004 Damien Miller
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
#include <sys/socket.h>
#include <sys/un.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_CTLSOCK		"/var/run/syslogd.sock"

#define MAX_MEMBUF_NAME	64	/* Max length of membuf log name */

struct ctl_cmd {
#define CMD_READ	1	/* Read out log */
#define CMD_READ_CLEAR	2	/* Read and clear log */
#define CMD_CLEAR	3	/* Clear log */
#define CMD_LIST	4	/* List available logs */
	int	cmd;
	char	logname[MAX_MEMBUF_NAME];
};

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "Usage: %s [-Ccq] [-s ctlsock] logname\n", __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	const char *ctlsock_path;
	char buf[8192];
	struct sockaddr_un ctl;
	socklen_t ctllen;
	int ctlsock, ch;
	FILE *ctlf;
	extern char *optarg;
	extern int optind;
	struct ctl_cmd cc;

	memset(&cc, '\0', sizeof(cc));

	ctlsock_path = DEFAULT_CTLSOCK;
	while ((ch = getopt(argc, argv, "Cchqs:")) != -1) {
		switch (ch) {
		case 'C':
			cc.cmd = CMD_CLEAR;
			break;
		case 'c':
			cc.cmd = CMD_READ_CLEAR;
			break;
		case 'h':
			usage();
			break;
		case 'q':
			cc.cmd = CMD_LIST;
			break;
		case 's':
			ctlsock_path = optarg;
			break;
		default:
			fprintf(stderr, "Invalid commandline option.\n");
			usage();
			break;
		}
	}

	if (cc.cmd == 0)
		cc.cmd = CMD_READ;

	if ((cc.cmd != CMD_LIST && optind != argc - 1) ||
	    (cc.cmd == CMD_LIST && optind != argc))
		usage();

	if (cc.cmd != CMD_LIST) {
		if (strlcpy(cc.logname, argv[optind], sizeof(cc.logname)) >=
		    sizeof(cc.logname))
			errx(1, "Specified log name is too long");
	}

	memset(&ctl, '\0', sizeof(ctl));
	strlcpy(ctl.sun_path, ctlsock_path, sizeof(ctl.sun_path));
	ctl.sun_family = AF_UNIX;

	if ((ctlsock = socket(PF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");
	if (connect(ctlsock, (struct sockaddr*)&ctl, sizeof(ctl)) == -1)
		err(1, "connect: %s", ctl.sun_path);
	if ((ctlf = fdopen(ctlsock, "r+")) == NULL)
		err(1, "fdopen");
	/* Send command */
	if (fwrite(&cc, sizeof(cc), 1, ctlf) != 1)
		err(1, "fwrite");

	fflush(ctlf);
	setlinebuf(ctlf);

	/* Write out reply */
	while((fgets(buf, sizeof(buf), ctlf)) != NULL)
		fputs(buf, stdout);

	fclose(ctlf);
	close(ctlsock);

	exit(0);
}
