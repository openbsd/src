/*	$OpenBSD: radiusd_bsdauth.c,v 1.4 2015/07/30 08:46:04 yasuoka Exp $	*/

/*
 * Copyright (c) 2015 YASUOKA Masahiko <yasuoka@yasuoka.net>
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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <bsd_auth.h>
#include <err.h>
#include <grp.h>
#include <imsg.h>
#include <login_cap.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "radiusd.h"
#include "radiusd_module.h"

struct module_bsdauth {
	struct module_base	 *base;
	struct imsgbuf		  ibuf;
	char			**okgroups;
};

/* IPC between priv and main */
enum {
	IMSG_BSDAUTH_OK = 1000,
	IMSG_BSDAUTH_NG,
	IMSG_BSDAUTH_USERCHECK,
	IMSG_BSDAUTH_GROUPCHECK
};
struct auth_usercheck_args {
	size_t	userlen;
	size_t	passlen;
};
struct auth_groupcheck_args {
	size_t	userlen;
	size_t	grouplen;
};

static void	 module_bsdauth_main(int, int);
static void	 module_bsdauth_config_set(void *, const char *, int,
		    char * const *);
static void	 module_bsdauth_userpass(void *, u_int, const char *,
		    const char *);
__dead static void
		 fatal(const char *);

static struct module_handlers module_bsdauth_handlers = {
	.userpass = module_bsdauth_userpass,
	.config_set = module_bsdauth_config_set
};

int
main(int argc, char *argv[])
{
	int		 pipe_chld, pairsock[2], status;
	struct imsgbuf	 ibuf;
	struct imsg	 imsg;
	ssize_t		 n;
	size_t		 datalen;

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pairsock) == -1)
		err(EXIT_FAILURE, "socketpair");

	pipe_chld = pairsock[1];
	module_bsdauth_main(pairsock[0], pairsock[1]);

	/*
	 * Privileged process
	 */
	openlog(NULL, LOG_PID, LOG_DAEMON);
	setproctitle("[priv]");
	imsg_init(&ibuf, pipe_chld);
	for (;;) {
		if ((n = imsg_read(&ibuf)) <= 0)
			break;
		for (;;) {
			if ((n = imsg_get(&ibuf, &imsg)) == -1)
				break;
			if (n == 0)
				break;
			datalen = imsg.hdr.len - IMSG_HEADER_SIZE;
			switch (imsg.hdr.type) {
			case IMSG_BSDAUTH_USERCHECK:
			    {
				char		*user, *pass;
				bool		 authok = false;
				struct auth_usercheck_args
						*args;

				if (datalen < sizeof(
				    struct auth_usercheck_args)) {
					syslog(LOG_ERR, "Short message");
					break;
				}
				args = (struct auth_usercheck_args *)imsg.data;

				if (datalen < sizeof(struct auth_usercheck_args)
				    + args->userlen + args->passlen) {
					syslog(LOG_ERR, "Short message");
					break;
				}
				user = (char *)(args + 1);
				user[args->userlen - 1] = '\0';
				pass = user + args->userlen;
				pass[args->passlen - 1] = '\0';

				if (auth_userokay(user, NULL, NULL, pass))
					authok = true;
				explicit_bzero(pass, args->passlen);

				imsg_compose(&ibuf, (authok)
				    ? IMSG_BSDAUTH_OK : IMSG_BSDAUTH_NG,
				    0, 0, -1, NULL, 0);
				break;
			    }
			case IMSG_BSDAUTH_GROUPCHECK:
			    {
				int		 i;
				char		*user, *group;
				struct passwd   *pw;
				struct group	 gr0, *gr;
				char		 g_buf[4096];
				bool		 group_ok = false;
				struct auth_groupcheck_args
						*args;

				if (datalen < sizeof(
				    struct auth_groupcheck_args)) {
					syslog(LOG_ERR, "Short message");
					break;
				}
				args = (struct auth_groupcheck_args *)imsg.data;
				if (datalen < sizeof(
					    struct auth_groupcheck_args) +
				    args->userlen + args->grouplen) {
					syslog(LOG_ERR, "Short message");
					break;
				}
				user = (char *)(args + 1);
				user[args->userlen - 1] = '\0';
				group = user + args->userlen;
				group[args->grouplen - 1] = '\0';

				pw = getpwnam(user);
				if (getgrnam_r(group, &gr0, g_buf,
				    sizeof(g_buf), &gr) == -1)
					goto group_done;

				if (gr->gr_gid == pw->pw_gid) {
					group_ok = true;
					goto group_done;
				}
				for (i = 0; gr->gr_mem[i] != NULL; i++) {
					if (strcmp(gr->gr_mem[i], pw->pw_name)
					    == 0) {
						group_ok = true;
						goto group_done;
					}
				}
group_done:
				endgrent();

				imsg_compose(&ibuf, (group_ok)
				    ? IMSG_BSDAUTH_OK : IMSG_BSDAUTH_NG,
				    0, 0, -1, NULL, 0);
				break;
			    }
			    imsg_free(&imsg);
			}
			imsg_flush(&ibuf);
		}
		imsg_flush(&ibuf);
	}
	imsg_clear(&ibuf);
	wait(&status);

	exit(WEXITSTATUS(status));
}

static void
module_bsdauth_main(int pipe_prnt, int pipe_chld)
{
	int			 i;
	pid_t			 pid;
	struct module_bsdauth	 module_bsdauth;

	pid = fork();
	if (pid == -1)
		err(EXIT_FAILURE, "fork");

	if (pid > 0) {
		close(pipe_prnt);
		return;
	}
	close(pipe_chld);

	/* main process */
	setproctitle("[main]");
	openlog(NULL, LOG_PID, LOG_DAEMON);
	memset(&module_bsdauth, 0, sizeof(module_bsdauth));
	if ((module_bsdauth.base = module_create(STDIN_FILENO, &module_bsdauth,
	    &module_bsdauth_handlers)) == NULL)
		err(1, "Could not create a module instance");

	module_drop_privilege(module_bsdauth.base);

	module_load(module_bsdauth.base);
	imsg_init(&module_bsdauth.ibuf, pipe_prnt);
	while (module_run(module_bsdauth.base) == 0)
		;

	module_destroy(module_bsdauth.base);
	imsg_clear(&module_bsdauth.ibuf);

	if (module_bsdauth.okgroups) {
		for (i = 0; module_bsdauth.okgroups[i] != NULL; i++)
			free(module_bsdauth.okgroups[i]);
	}
	free(module_bsdauth.okgroups);

	_exit(EXIT_SUCCESS);
}

static void
module_bsdauth_config_set(void *ctx, const char *name, int argc,
    char * const * argv)
{
	struct module_bsdauth	 *module = ctx;
	int			  i;
	char			**groups = NULL;

	if (strcmp(name, "restrict-group") == 0) {
		if (module->okgroups != NULL) {
			module_send_message(module->base, IMSG_NG,
			    "`restrict-group' is already defined");
			goto on_error;
		}
		if ((groups = calloc(sizeof(char *), argc + 1)) == NULL) {
			module_send_message(module->base, IMSG_NG,
			    "Out of memory");
			goto on_error;
		}
		for (i = 0; i < argc; i++) {
			if ((groups[i] = strdup(argv[i])) == NULL) {
				module_send_message(module->base,
				    IMSG_NG, "Out of memory");
				goto on_error;
			}
		}
		groups[i] = NULL;
		module->okgroups = groups;
		module_send_message(module->base, IMSG_OK, NULL);
	} else
		module_send_message(module->base, IMSG_NG,
		    "Unknown config parameter `%s'", name);
	return;
on_error:
	if (groups != NULL) {
		for (i = 0; groups[i] != NULL; i++)
			free(groups[i]);
		free(groups);
	}
	return;
}


static void
module_bsdauth_userpass(void *ctx, u_int q_id, const char *user,
    const char *pass)
{
	struct module_bsdauth	*module = ctx;
	struct auth_usercheck_args
				 usercheck;
	struct auth_groupcheck_args
				 groupcheck;
	struct iovec		iov[4];
	const char		*group;
	u_int			 i;
	const char		*reason;
	struct imsg		 imsg;
	ssize_t			 n;

	memset(&imsg, 0, sizeof(imsg));
	if (pass == NULL)
		pass = "";

	usercheck.userlen = strlen(user) + 1;
	usercheck.passlen = strlen(pass) + 1;
	iov[0].iov_base = &usercheck;
	iov[0].iov_len = sizeof(usercheck);
	iov[1].iov_base = (char *)user;
	iov[1].iov_len = usercheck.userlen;
	iov[2].iov_base = (char *)pass;
	iov[2].iov_len = usercheck.passlen;

	imsg_composev(&module->ibuf, IMSG_BSDAUTH_USERCHECK, 0, 0, -1, iov, 3);
	imsg_flush(&module->ibuf);
	if ((n = imsg_read(&module->ibuf)) == -1 || n == 0)
		fatal("imsg_read() failed in module_bsdauth_userpass()");
	if ((n = imsg_get(&module->ibuf, &imsg)) <= 0)
		fatal("imsg_get() failed in module_bsdauth_userpass()");

	if (imsg.hdr.type != IMSG_BSDAUTH_OK) {
		reason = "Authentication failed";
		goto auth_ng;
	}
	if (module->okgroups != NULL) {
		reason = "Group restriction is not allowed";
		for (i = 0; module->okgroups[i] != NULL; i++) {
			group = module->okgroups[i];

			groupcheck.userlen = strlen(user) + 1;
			groupcheck.grouplen = strlen(group) + 1;
			iov[0].iov_base = &groupcheck;
			iov[0].iov_len = sizeof(groupcheck);
			iov[1].iov_base = (char *)user;
			iov[1].iov_len = groupcheck.userlen;
			iov[2].iov_base = (char *)group;
			iov[2].iov_len = groupcheck.grouplen;
			imsg_composev(&module->ibuf, IMSG_BSDAUTH_GROUPCHECK,
			    0, 0, -1, iov, 3);
			imsg_flush(&module->ibuf);
			if ((n = imsg_read(&module->ibuf)) == -1 || n == 0)
				fatal("imsg_read() failed in "
				    "module_bsdauth_userpass()");
			if ((n = imsg_get(&module->ibuf, &imsg)) <= 0)
				fatal("imsg_get() failed in "
				    "module_bsdauth_userpass()");
			if (imsg.hdr.type == IMSG_BSDAUTH_OK)
				goto group_ok;
		}
		goto auth_ng;
	}
group_ok:
	module_userpass_ok(module->base, q_id, "Authentication succeeded");
	imsg_free(&imsg);
	return;
auth_ng:
	module_userpass_fail(module->base, q_id, reason);
	imsg_free(&imsg);
	return;
}

static void
fatal(const char *msg)
{
	syslog(LOG_ERR, "%s: %m", msg);
	abort();
}
