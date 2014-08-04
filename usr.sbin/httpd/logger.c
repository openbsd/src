/*	$OpenBSD: logger.c,v 1.1 2014/08/04 15:49:28 reyk Exp $	*/

/*
 * Copyright (c) 2014 Reyk Floeter <reyk@openbsd.org>
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

#include <net/if.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <event.h>

#include "httpd.h"

int		 logger_dispatch_parent(int, struct privsep_proc *,
		    struct imsg *);
int		 logger_dispatch_server(int, struct privsep_proc *,
		    struct imsg *);
void		 logger_shutdown(void);
void		 logger_close(void);
void		 logger_init(struct privsep *, struct privsep_proc *p, void *);
int		 logger_start(void);
int		 logger_log(struct imsg *);

static struct httpd		*env = NULL;
int				 proc_id;
int				 log_fd = -1;
int				 error_fd = -1;

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	logger_dispatch_parent },
	{ "server",	PROC_SERVER,	logger_dispatch_server }
};

pid_t
logger(struct privsep *ps, struct privsep_proc *p)
{
	env = ps->ps_env;
	return (proc_run(ps, p, procs, nitems(procs), logger_init, NULL));
}

void
logger_shutdown(void)
{
	logger_close();
	config_purge(env, CONFIG_ALL);
}

void
logger_close(void)
{
	if (log_fd != -1) {
		close(log_fd);
		log_fd = -1;
	}
	if (error_fd != -1) {
		close(error_fd);
		error_fd = -1;
	}
}

void
logger_init(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	if (config_init(ps->ps_env) == -1)
		fatal("failed to initialize configuration");

	/* Set to current prefork id */
	proc_id = p->p_instance;

	/* We use a custom shutdown callback */
	p->p_shutdown = logger_shutdown;
}

int
logger_start(void)
{
	logger_close();
	if ((log_fd = open(HTTPD_ACCESS_LOG,
	    O_WRONLY|O_APPEND|O_CREAT, 0644)) == -1)
		return (-1);
	if ((error_fd = open(HTTPD_ERROR_LOG,
	    O_WRONLY|O_APPEND|O_CREAT, 0644)) == -1)
		return (-1);
	return (0);
}

int
logger_log(struct imsg *imsg)
{
	char	*logline;
	int	 fd;

	if (imsg->hdr.type == IMSG_LOG_ACCESS)
		fd = log_fd;
	else
		fd = error_fd;

	/* XXX get_string() would sanitize the string, but add a malloc */
	logline = imsg->data;

	/* For debug output */
	log_debug("%s", logline);

	if (dprintf(fd, "%s\n", logline) == -1) {
		if (logger_start() == -1)
			return (-1);
	}

	return (0);
}

int
logger_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	switch (imsg->hdr.type) {
	case IMSG_CFG_DONE:
		config_getcfg(env, imsg);
		break;
	case IMSG_CTL_START:
	case IMSG_CTL_REOPEN:
		return (logger_start());
	case IMSG_CTL_RESET:
		config_getreset(env, imsg);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
logger_dispatch_server(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	switch (imsg->hdr.type) {
	case IMSG_LOG_ACCESS:
		return (logger_log(imsg));
	case IMSG_LOG_ERROR:
		return (logger_log(imsg));
	default:
		return (-1);
	}

	return (0);
}
