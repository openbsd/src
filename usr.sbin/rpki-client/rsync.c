/*	$OpenBSD: rsync.c,v 1.9 2020/09/12 15:46:48 claudio Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <poll.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * A running rsync process.
 * We can have multiple of these simultaneously and need to keep track
 * of which process maps to which request.
 */
struct	rsyncproc {
	char	*uri; /* uri of this rsync proc */
	size_t	 id; /* identity of request */
	pid_t	 pid; /* pid of process or 0 if unassociated */
};

/*
 * Conforms to RFC 5781.
 * Note that "Source" is broken down into the module, path, and also
 * file type relevant to RPKI.
 * Any of the pointers (except "uri") may be NULL.
 * Returns zero on failure, non-zero on success.
 */
int
rsync_uri_parse(const char **hostp, size_t *hostsz,
    const char **modulep, size_t *modulesz,
    const char **pathp, size_t *pathsz,
    enum rtype *rtypep, const char *uri)
{
	const char	*host, *module, *path;
	size_t		 sz;

	/* Initialise all output values to NULL or 0. */

	if (hostsz != NULL)
		*hostsz = 0;
	if (modulesz != NULL)
		*modulesz = 0;
	if (pathsz != NULL)
		*pathsz = 0;
	if (hostp != NULL)
		*hostp = 0;
	if (modulep != NULL)
		*modulep = 0;
	if (pathp != NULL)
		*pathp = 0;
	if (rtypep != NULL)
		*rtypep = RTYPE_EOF;

	/* Case-insensitive rsync URI. */

	if (strncasecmp(uri, "rsync://", 8)) {
		warnx("%s: not using rsync schema", uri);
		return 0;
	}

	/* Parse the non-zero-length hostname. */

	host = uri + 8;

	if ((module = strchr(host, '/')) == NULL) {
		warnx("%s: missing rsync module", uri);
		return 0;
	} else if (module == host) {
		warnx("%s: zero-length rsync host", uri);
		return 0;
	}

	if (hostp != NULL)
		*hostp = host;
	if (hostsz != NULL)
		*hostsz = module - host;

	/* The non-zero-length module follows the hostname. */

	if (module[1] == '\0') {
		warnx("%s: zero-length rsync module", uri);
		return 0;
	}

	module++;

	/* The path component is optional. */

	if ((path = strchr(module, '/')) == NULL) {
		assert(*module != '\0');
		if (modulep != NULL)
			*modulep = module;
		if (modulesz != NULL)
			*modulesz = strlen(module);
		return 1;
	} else if (path == module) {
		warnx("%s: zero-length module", uri);
		return 0;
	}

	if (modulep != NULL)
		*modulep = module;
	if (modulesz != NULL)
		*modulesz = path - module;

	path++;
	sz = strlen(path);

	if (pathp != NULL)
		*pathp = path;
	if (pathsz != NULL)
		*pathsz = sz;

	if (rtypep != NULL && sz > 4) {
		if (strcasecmp(path + sz - 4, ".roa") == 0)
			*rtypep = RTYPE_ROA;
		else if (strcasecmp(path + sz - 4, ".mft") == 0)
			*rtypep = RTYPE_MFT;
		else if (strcasecmp(path + sz - 4, ".cer") == 0)
			*rtypep = RTYPE_CER;
		else if (strcasecmp(path + sz - 4, ".crl") == 0)
			*rtypep = RTYPE_CRL;
	}

	return 1;
}

static void
proc_child(int signal)
{

	/* Nothing: just discard. */
}

/*
 * Process used for synchronising repositories.
 * This simply waits to be told which repository to synchronise, then
 * does so.
 * It then responds with the identifier of the repo that it updated.
 * It only exits cleanly when fd is closed.
 * FIXME: this should use buffered output to prevent deadlocks, but it's
 * very unlikely that we're going to fill our buffer, so whatever.
 * FIXME: limit the number of simultaneous process.
 * Currently, an attacker can trivially specify thousands of different
 * repositories and saturate our system.
 */
void
proc_rsync(char *prog, char *bind_addr, int fd)
{
	size_t			 id, i, idsz = 0;
	ssize_t			 ssz;
	char			*host = NULL, *mod = NULL, *uri = NULL,
				*dst = NULL, *path, *save, *cmd;
	const char		*pp;
	pid_t			 pid;
	char			*args[32];
	int			 st, rc = 0;
	struct stat		 stt;
	struct pollfd		 pfd;
	sigset_t		 mask, oldmask;
	struct rsyncproc	*ids = NULL;

	pfd.fd = fd;
	pfd.events = POLLIN;

	/*
	 * Unveil the command we want to run.
	 * If this has a pathname component in it, interpret as a file
	 * and unveil the file directly.
	 * Otherwise, look up the command in our PATH.
	 */

	if (strchr(prog, '/') == NULL) {
		if (getenv("PATH") == NULL)
			errx(1, "PATH is unset");
		if ((path = strdup(getenv("PATH"))) == NULL)
			err(1, "strdup");
		save = path;
		while ((pp = strsep(&path, ":")) != NULL) {
			if (*pp == '\0')
				continue;
			if (asprintf(&cmd, "%s/%s", pp, prog) == -1)
				err(1, "asprintf");
			if (lstat(cmd, &stt) == -1) {
				free(cmd);
				continue;
			} else if (unveil(cmd, "x") == -1)
				err(1, "%s: unveil", cmd);
			free(cmd);
			break;
		}
		free(save);
	} else if (unveil(prog, "x") == -1)
		err(1, "%s: unveil", prog);

	/* Unveil the repository directory and terminate unveiling. */

	if (unveil(".", "c") == -1)
		err(1, "unveil");
	if (unveil(NULL, NULL) == -1)
		err(1, "unveil");

	/* Initialise retriever for children exiting. */

	if (sigemptyset(&mask) == -1)
		err(1, NULL);
	if (signal(SIGCHLD, proc_child) == SIG_ERR)
		err(1, NULL);
	if (sigaddset(&mask, SIGCHLD) == -1)
		err(1, NULL);
	if (sigprocmask(SIG_BLOCK, &mask, &oldmask) == -1)
		err(1, NULL);

	for (;;) {
		if (ppoll(&pfd, 1, NULL, &oldmask) == -1) {
			if (errno != EINTR)
				err(1, "ppoll");

			/*
			 * If we've received an EINTR, it means that one
			 * of our children has exited and we can reap it
			 * and look up its identifier.
			 * Then we respond to the parent.
			 */

			while ((pid = waitpid(WAIT_ANY, &st, WNOHANG)) > 0) {
				int ok = 1;

				for (i = 0; i < idsz; i++)
					if (ids[i].pid == pid)
						break;
				assert(i < idsz);

				if (!WIFEXITED(st)) {
					warnx("rsync %s terminated abnormally",
					    ids[i].uri);
					rc = 1;
					ok = 0;
				} else if (WEXITSTATUS(st) != 0) {
					warnx("rsync %s failed", ids[i].uri);
					ok = 0;
				}

				io_simple_write(fd, &ids[i].id, sizeof(size_t));
				io_simple_write(fd, &ok, sizeof(ok));
				free(ids[i].uri);
				ids[i].uri = NULL;
				ids[i].pid = 0;
				ids[i].id = 0;
			}
			if (pid == -1 && errno != ECHILD)
				err(1, "waitpid");
			continue;
		}

		/*
		 * Read til the parent exits.
		 * That will mean that we can safely exit.
		 */

		if ((ssz = read(fd, &id, sizeof(size_t))) == -1)
			err(1, "read");
		if (ssz == 0)
			break;

		/* Read host and module. */

		io_str_read(fd, &host);
		io_str_read(fd, &mod);

		/*
		 * Create source and destination locations.
		 * Build up the tree to this point because GPL rsync(1)
		 * will not build the destination for us.
		 */

		if (mkdir(host, 0700) == -1 && EEXIST != errno)
			err(1, "%s", host);

		if (asprintf(&dst, "%s/%s", host, mod) == -1)
			err(1, NULL);
		if (mkdir(dst, 0700) == -1 && EEXIST != errno)
			err(1, "%s", dst);

		if (asprintf(&uri, "rsync://%s/%s", host, mod) == -1)
			err(1, NULL);

		/* Run process itself, wait for exit, check error. */

		if ((pid = fork()) == -1)
			err(1, "fork");

		if (pid == 0) {
			if (pledge("stdio exec", NULL) == -1)
				err(1, "pledge");
			i = 0;
			args[i++] = (char *)prog;
			args[i++] = "-rt";
			if (bind_addr != NULL) {
				args[i++] = "--address";
				args[i++] = (char *)bind_addr;
			}
			args[i++] = uri;
			args[i++] = dst;
			args[i] = NULL;
			execvp(args[0], args);
			err(1, "%s: execvp", prog);
		}

		/* Augment the list of running processes. */

		for (i = 0; i < idsz; i++)
			if (ids[i].pid == 0)
				break;
		if (i == idsz) {
			ids = reallocarray(ids, idsz + 1, sizeof(*ids));
			if (ids == NULL)
				err(1, NULL);
			idsz++;
		}

		ids[i].id = id;
		ids[i].pid = pid;
		ids[i].uri = uri;

		/* Clean up temporary values. */

		free(mod);
		free(dst);
		free(host);
	}

	/* No need for these to be hanging around. */
	for (i = 0; i < idsz; i++)
		if (ids[i].pid > 0) {
			kill(ids[i].pid, SIGTERM);
			free(ids[i].uri);
		}

	free(ids);
	exit(rc);
	/* NOTREACHED */
}
