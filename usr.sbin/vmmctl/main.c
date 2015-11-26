/*	$OpenBSD: main.c,v 1.3 2015/11/26 08:26:48 reyk Exp $	*/

/*
 * Copyright (c) 2015 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/queue.h>
#include <sys/un.h>
#include <sys/cdefs.h>

#include <machine/vmmvar.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <imsg.h>

#include "vmd.h"
#include "parser.h"

static const char	*socket_name = SOCKET_NAME;

__dead void	 usage(void);
int		 vmm_action(struct parse_result *);

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-q] [-s socket] command [arg ...]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct parse_result	*res;
	int			 ch;
	int			 ret;
	const char		*paths[2];

	while ((ch = getopt(argc, argv, "s:")) != -1) {
		switch (ch) {
		case 's':
			socket_name = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	/* parse command line options */
	if ((res = parse(argc, argv)) == NULL)
		exit(1);

	switch (res->action) {
	case NONE:
		usage();
		break;
	case CMD_CREATE:
		paths[0] = res->path;
		paths[1] = NULL;
		if (pledge("stdio rpath wpath cpath", paths) == -1)
			err(1, "pledge");
		if (res->size < 1)
			errx(1, "specified image size too small");

		ret = create_imagefile(res->path, res->size);
		if (ret != 0) {
			errno = ret;
			err(1, "create imagefile operation failed");
		} else
			warnx("imagefile created");
		return (0);
	case CMD_LOAD:
		if (pledge("stdio rpath unix", NULL) == -1)
			err(1, "pledge");

		/* parse configuration file options */
		if (res->path == NULL)
			ret = parse_config(VMM_CONF);
		else
			ret = parse_config(res->path);
		break;
	default:
		if (pledge("stdio unix", NULL) == -1)
			err(1, "pledge");
		ret = vmmaction(res);
		break;
	}

	return (ret);
}

int
vmmaction(struct parse_result *res)
{
	struct sockaddr_un	 sun;
	struct imsg		 imsg;
	int			 done = 0;
	int			 n;
	int			 ret;
	int			 ctl_sock;

	/*
	 * Connect to vmd control socket.
	 * XXX vmd currently only accepts one request per connection,
	 * XXX so we have to open the control socket each time this
	 * XXX function is called.  This should be changed later.
	 */
	if ((ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, socket_name, sizeof(sun.sun_path));

	if (connect(ctl_sock, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "connect: %s", socket_name);

	if ((ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
		err(1, "malloc");
	imsg_init(ibuf, ctl_sock);

	switch (res->action) {
	case CMD_START:
		/* XXX validation should be done in start_vm() */
		if (res->size < 1)
			errx(1, "specified memory size too small");
		if (res->path == NULL)
			errx(1, "no kernel specified");
		if (res->ndisks > VMM_MAX_DISKS_PER_VM)
			errx(1, "too many disks");
		else if (res->ndisks == 0)
			warnx("starting without disks");

		ret = start_vm(res->name, res->size, res->nifs,
		    res->ndisks, res->disks, res->path);
		if (ret) {
			errno = ret;
			err(1, "start VM operation failed");
		}
		break;
	case CMD_TERMINATE:
		terminate_vm(res->id);
		break;
	case CMD_INFO:
		get_info_vm(res->id);
		break;
	case CMD_CREATE:
	case CMD_LOAD:
	case NONE:
		break;
	}

	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) <= 0 && errno != EAGAIN)
			err(1, "write error");

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		while (!done) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				errx(1, "imsg_get error");
			if (n == 0)
				break;

			ret = 0;
			switch (res->action) {
			case CMD_START:
				done = start_vm_complete(&imsg, &ret);
				break;
			case CMD_TERMINATE:
				done = terminate_vm_complete(&imsg, &ret);
				break;
			case CMD_INFO:
				done = add_info(&imsg, &ret);
				break;
			default:
				done = 1;
				break;
			}

			imsg_free(&imsg);
		}
	}

	close(ibuf->fd);
	free(ibuf);

	return (0);
}

int
parse_ifs(struct parse_result *res, char *word, int val)
{
	const char	*error;

	if (word != NULL) {
		val = strtonum(word, 0, INT_MAX, &error);
		if (error != NULL)  {
			warnx("invalid count \"%s\": %s", word, error);
			return (-1);
		}
	}
	res->nifs = val;
	return (0);
}

int
parse_size(struct parse_result *res, char *word,
    long long val)
{
	char		*s = "";

	if (word != NULL) {
		val = strtol(word, &s, 10);
		if (errno == ERANGE &&
		    (val == LLONG_MIN || val == LLONG_MAX)) {
			warnx("out of range: %s", word);
			return (-1);
		}
	}

	/* Convert to megabytes */
	if (*s == '\0')
		res->size = val / 1024 / 1024;
	else if (strcmp(s, "K") == 0)
		res->size = val / 1024;
	else if (strcmp(s, "M") == 0)
		res->size = val;
	else if (strcmp(s, "G") == 0)
		res->size = val * 1024;
	else {
		warnx("invalid unit: %s", s);
		return (-1);
	}
	if (res->size > LLONG_MAX) {
		warnx("size too large: %s", word);
		return (-1);
	}

	return (0);
}

int
parse_disk(struct parse_result *res, char *word)
{
	char		**disks;
	char		*s;

	if ((disks = reallocarray(res->disks, res->ndisks + 1,
	    sizeof(char *))) == NULL) {
		warn("reallocarray");
		return (-1);
	}
	if ((s = strdup(word)) == NULL) {
		warn("strdup");
		return (-1);
	}
	disks[res->ndisks] = s;
	res->disks = disks;
	res->ndisks++;

	return (0);
}

int
parse_vmid(struct parse_result *res, char *word, uint32_t id)
{
	const char	*error;

	if (word != NULL) {
		id = strtonum(word, 0, UINT32_MAX, &error);
		if (error != NULL)  {
			warnx("invalid id: %s", error);
			return (-1);
		}
	}
	res->id = id;

	return (0);
}
