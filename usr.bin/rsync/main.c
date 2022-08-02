/*	$OpenBSD: main.c,v 1.65 2022/08/02 20:01:12 tb Exp $ */
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
#include <sys/socket.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "extern.h"

int verbose;
int poll_contimeout;
int poll_timeout;

/*
 * A remote host is has a colon before the first path separator.
 * This works for rsh remote hosts (host:/foo/bar), implicit rsync
 * remote hosts (host::/foo/bar), and explicit (rsync://host/foo).
 * Return zero if local, non-zero if remote.
 */
static int
fargs_is_remote(const char *v)
{
	size_t	 pos;

	pos = strcspn(v, ":/");
	return v[pos] == ':';
}

/*
 * Test whether a remote host is specifically an rsync daemon.
 * Return zero if not, non-zero if so.
 */
static int
fargs_is_daemon(const char *v)
{
	size_t	 pos;

	if (strncasecmp(v, "rsync://", 8) == 0)
		return 1;

	pos = strcspn(v, ":/");
	return v[pos] == ':' && v[pos + 1] == ':';
}

/*
 * Take the command-line filenames (e.g., rsync foo/ bar/ baz/) and
 * determine our operating mode.
 * For example, if the first argument is a remote file, this means that
 * we're going to transfer from the remote to the local.
 * We also make sure that the arguments are consistent, that is, if
 * we're going to transfer from the local to the remote, that no
 * filenames for the local transfer indicate remote hosts.
 * Always returns the parsed and sanitised options.
 */
static struct fargs *
fargs_parse(size_t argc, char *argv[], struct opts *opts)
{
	struct fargs	*f = NULL;
	char		*cp, *ccp;
	size_t		 i, j, len = 0;

	/* Allocations. */

	if ((f = calloc(1, sizeof(struct fargs))) == NULL)
		err(ERR_NOMEM, NULL);

	f->sourcesz = argc - 1;
	if ((f->sources = calloc(f->sourcesz, sizeof(char *))) == NULL)
		err(ERR_NOMEM, NULL);

	for (i = 0; i < argc - 1; i++)
		if ((f->sources[i] = strdup(argv[i])) == NULL)
			err(ERR_NOMEM, NULL);

	if ((f->sink = strdup(argv[i])) == NULL)
		err(ERR_NOMEM, NULL);

	/*
	 * Test files for its locality.
	 * If the last is a remote host, then we're sending from the
	 * local to the remote host ("sender" mode).
	 * If the first, remote to local ("receiver" mode).
	 * If neither, a local transfer in sender style.
	 */

	f->mode = FARGS_SENDER;

	if (fargs_is_remote(f->sink)) {
		f->mode = FARGS_SENDER;
		if ((f->host = strdup(f->sink)) == NULL)
			err(ERR_NOMEM, NULL);
	}

	if (fargs_is_remote(f->sources[0])) {
		if (f->host != NULL)
			errx(ERR_SYNTAX, "both source and destination "
			    "cannot be remote files");
		f->mode = FARGS_RECEIVER;
		if ((f->host = strdup(f->sources[0])) == NULL)
			err(ERR_NOMEM, NULL);
	}

	if (f->host != NULL) {
		if (strncasecmp(f->host, "rsync://", 8) == 0) {
			/* rsync://host[:port]/module[/path] */
			f->remote = 1;
			len = strlen(f->host) - 8 + 1;
			memmove(f->host, f->host + 8, len);
			if ((cp = strchr(f->host, '/')) == NULL)
				errx(ERR_SYNTAX,
				    "rsync protocol requires a module name");
			*cp++ = '\0';
			f->module = cp;
			if ((cp = strchr(f->module, '/')) != NULL)
				*cp = '\0';
			if ((cp = strchr(f->host, ':')) != NULL) {
				/* host:port --> extract port */
				*cp++ = '\0';
				opts->port = cp;
			}
		} else {
			/* host:[/path] */
			cp = strchr(f->host, ':');
			assert(cp != NULL);
			*cp++ = '\0';
			if (*cp == ':') {
				/* host::module[/path] */
				f->remote = 1;
				f->module = ++cp;
				cp = strchr(f->module, '/');
				if (cp != NULL)
					*cp = '\0';
			}
		}
		if ((len = strlen(f->host)) == 0)
			errx(ERR_SYNTAX, "empty remote host");
		if (f->remote && strlen(f->module) == 0)
			errx(ERR_SYNTAX, "empty remote module");
	}

	/* Make sure we have the same "hostspec" for all files. */

	if (!f->remote) {
		if (f->mode == FARGS_SENDER)
			for (i = 0; i < f->sourcesz; i++) {
				if (!fargs_is_remote(f->sources[i]))
					continue;
				errx(ERR_SYNTAX,
				    "remote file in list of local sources: %s",
				    f->sources[i]);
			}
		if (f->mode == FARGS_RECEIVER)
			for (i = 0; i < f->sourcesz; i++) {
				if (fargs_is_remote(f->sources[i]) &&
				    !fargs_is_daemon(f->sources[i]))
					continue;
				if (fargs_is_daemon(f->sources[i]))
					errx(ERR_SYNTAX,
					    "remote daemon in list of remote "
					    "sources: %s", f->sources[i]);
				errx(ERR_SYNTAX, "local file in list of "
				    "remote sources: %s", f->sources[i]);
			}
	} else {
		if (f->mode != FARGS_RECEIVER)
			errx(ERR_SYNTAX, "sender mode for remote "
				"daemon receivers not yet supported");
		for (i = 0; i < f->sourcesz; i++) {
			if (fargs_is_daemon(f->sources[i]))
				continue;
			errx(ERR_SYNTAX, "non-remote daemon file "
				"in list of remote daemon sources: "
				"%s", f->sources[i]);
		}
	}

	/*
	 * If we're not remote and a sender, strip our hostname.
	 * Then exit if we're a sender or a local connection.
	 */

	if (!f->remote) {
		if (f->host == NULL)
			return f;
		if (f->mode == FARGS_SENDER) {
			assert(f->host != NULL);
			assert(len > 0);
			j = strlen(f->sink);
			memmove(f->sink, f->sink + len + 1, j - len);
			return f;
		} else if (f->mode != FARGS_RECEIVER)
			return f;
	}

	/*
	 * Now strip the hostnames from the remote host.
	 *   rsync://host/module/path -> module/path
	 *   host::module/path -> module/path
	 *   host:path -> path
	 * Also make sure that the remote hosts are the same.
	 */

	assert(f->host != NULL);
	assert(len > 0);

	for (i = 0; i < f->sourcesz; i++) {
		cp = f->sources[i];
		j = strlen(cp);
		if (f->remote &&
		    strncasecmp(cp, "rsync://", 8) == 0) {
			/* rsync://path */
			cp += 8;
			if ((ccp = strchr(cp, ':')))	/* skip :port */
				*ccp = '\0';
			if (strncmp(cp, f->host, len) ||
			    (cp[len] != '/' && cp[len] != '\0'))
				errx(ERR_SYNTAX, "different remote host: %s",
				    f->sources[i]);
			memmove(f->sources[i],
				f->sources[i] + len + 8 + 1,
				j - len - 8);
		} else if (f->remote && strncmp(cp, "::", 2) == 0) {
			/* ::path */
			memmove(f->sources[i],
				f->sources[i] + 2, j - 1);
		} else if (f->remote) {
			/* host::path */
			if (strncmp(cp, f->host, len) ||
			    (cp[len] != ':' && cp[len] != '\0'))
				errx(ERR_SYNTAX, "different remote host: %s",
				    f->sources[i]);
			memmove(f->sources[i], f->sources[i] + len + 2,
			    j - len - 1);
		} else if (cp[0] == ':') {
			/* :path */
			memmove(f->sources[i], f->sources[i] + 1, j);
		} else {
			/* host:path */
			if (strncmp(cp, f->host, len) ||
			    (cp[len] != ':' && cp[len] != '\0'))
				errx(ERR_SYNTAX, "different remote host: %s",
				    f->sources[i]);
			memmove(f->sources[i],
				f->sources[i] + len + 1, j - len);
		}
	}

	return f;
}

static struct opts	 opts;

#define OP_ADDRESS	1000
#define OP_PORT		1001
#define OP_RSYNCPATH	1002
#define OP_TIMEOUT	1003
#define OP_VERSION	1004
#define OP_EXCLUDE	1005
#define OP_INCLUDE	1006
#define OP_EXCLUDE_FROM	1007
#define OP_INCLUDE_FROM	1008
#define OP_COMP_DEST	1009
#define OP_COPY_DEST	1010
#define OP_LINK_DEST	1011
#define OP_MAX_SIZE	1012
#define OP_MIN_SIZE	1013
#define OP_CONTIMEOUT	1014

const struct option	 lopts[] = {
    { "address",	required_argument, NULL,		OP_ADDRESS },
    { "archive",	no_argument,	NULL,			'a' },
    { "compare-dest",	required_argument, NULL,		OP_COMP_DEST },
#if 0
    { "copy-dest",	required_argument, NULL,		OP_COPY_DEST },
    { "link-dest",	required_argument, NULL,		OP_LINK_DEST },
#endif
    { "compress",	no_argument,	NULL,			'z' },
    { "contimeout",	required_argument, NULL,		OP_CONTIMEOUT },
    { "del",		no_argument,	&opts.del,		1 },
    { "delete",		no_argument,	&opts.del,		1 },
    { "devices",	no_argument,	&opts.devices,		1 },
    { "no-devices",	no_argument,	&opts.devices,		0 },
    { "dry-run",	no_argument,	&opts.dry_run,		1 },
    { "exclude",	required_argument, NULL,		OP_EXCLUDE },
    { "exclude-from",	required_argument, NULL,		OP_EXCLUDE_FROM },
    { "group",		no_argument,	&opts.preserve_gids,	1 },
    { "no-group",	no_argument,	&opts.preserve_gids,	0 },
    { "help",		no_argument,	NULL,			'h' },
    { "include",	required_argument, NULL,		OP_INCLUDE },
    { "include-from",	required_argument, NULL,		OP_INCLUDE_FROM },
    { "links",		no_argument,	&opts.preserve_links,	1 },
    { "max-size",	required_argument, NULL,		OP_MAX_SIZE },
    { "min-size",	required_argument, NULL,		OP_MIN_SIZE },
    { "no-links",	no_argument,	&opts.preserve_links,	0 },
    { "no-motd",	no_argument,	&opts.no_motd,		1 },
    { "numeric-ids",	no_argument,	&opts.numeric_ids,	1 },
    { "owner",		no_argument,	&opts.preserve_uids,	1 },
    { "no-owner",	no_argument,	&opts.preserve_uids,	0 },
    { "perms",		no_argument,	&opts.preserve_perms,	1 },
    { "no-perms",	no_argument,	&opts.preserve_perms,	0 },
    { "port",		required_argument, NULL,		OP_PORT },
    { "recursive",	no_argument,	&opts.recursive,	1 },
    { "no-recursive",	no_argument,	&opts.recursive,	0 },
    { "rsh",		required_argument, NULL,		'e' },
    { "rsync-path",	required_argument, NULL,		OP_RSYNCPATH },
    { "sender",		no_argument,	&opts.sender,		1 },
    { "server",		no_argument,	&opts.server,		1 },
    { "specials",	no_argument,	&opts.specials,		1 },
    { "no-specials",	no_argument,	&opts.specials,		0 },
    { "timeout",	required_argument, NULL,		OP_TIMEOUT },
    { "times",		no_argument,	&opts.preserve_times,	1 },
    { "no-times",	no_argument,	&opts.preserve_times,	0 },
    { "verbose",	no_argument,	&verbose,		1 },
    { "no-verbose",	no_argument,	&verbose,		0 },
    { "version",	no_argument,	NULL,			OP_VERSION },
    { NULL,		0,		NULL,			0 }
};

int
main(int argc, char *argv[])
{
	pid_t		 child;
	int		 fds[2], sd = -1, rc, c, st, i, lidx;
	size_t		 basedir_cnt = 0;
	struct sess	 sess;
	struct fargs	*fargs;
	char		**args;
	const char	*errstr;

	/* Global pledge. */

	if (pledge("stdio unix rpath wpath cpath dpath inet fattr chown dns getpw proc exec unveil",
	    NULL) == -1)
		err(ERR_IPC, "pledge");

	opts.max_size = opts.min_size = -1;

	while ((c = getopt_long(argc, argv, "Dae:ghlnoprtvxz", lopts, &lidx))
	    != -1) {
		switch (c) {
		case 'D':
			opts.devices = 1;
			opts.specials = 1;
			break;
		case 'a':
			opts.recursive = 1;
			opts.preserve_links = 1;
			opts.preserve_perms = 1;
			opts.preserve_times = 1;
			opts.preserve_gids = 1;
			opts.preserve_uids = 1;
			opts.devices = 1;
			opts.specials = 1;
			break;
		case 'e':
			opts.ssh_prog = optarg;
			break;
		case 'g':
			opts.preserve_gids = 1;
			break;
		case 'l':
			opts.preserve_links = 1;
			break;
		case 'n':
			opts.dry_run = 1;
			break;
		case 'o':
			opts.preserve_uids = 1;
			break;
		case 'p':
			opts.preserve_perms = 1;
			break;
		case 'r':
			opts.recursive = 1;
			break;
		case 't':
			opts.preserve_times = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'x':
			opts.one_file_system++;
			break;
		case 'z':
			fprintf(stderr, "%s: -z not supported yet\n", getprogname());
			break;
		case 0:
			/* Non-NULL flag values (e.g., --sender). */
			break;
		case OP_ADDRESS:
			opts.address = optarg;
			break;
		case OP_CONTIMEOUT:
			poll_contimeout = strtonum(optarg, 0, 60*60, &errstr);
			if (errstr != NULL)
				errx(ERR_SYNTAX, "timeout is %s: %s",
				    errstr, optarg);
			break;
		case OP_PORT:
			opts.port = optarg;
			break;
		case OP_RSYNCPATH:
			opts.rsync_path = optarg;
			break;
		case OP_TIMEOUT:
			poll_timeout = strtonum(optarg, 0, 60*60, &errstr);
			if (errstr != NULL)
				errx(ERR_SYNTAX, "timeout is %s: %s",
				    errstr, optarg);
			break;
		case OP_EXCLUDE:
			if (parse_rule(optarg, RULE_EXCLUDE) == -1)
				errx(ERR_SYNTAX, "syntax error in exclude: %s",
				    optarg);
			break;
		case OP_INCLUDE:
			if (parse_rule(optarg, RULE_INCLUDE) == -1)
				errx(ERR_SYNTAX, "syntax error in include: %s",
				    optarg);
			break;
		case OP_EXCLUDE_FROM:
			parse_file(optarg, RULE_EXCLUDE);
			break;
		case OP_INCLUDE_FROM:
			parse_file(optarg, RULE_INCLUDE);
			break;
		case OP_COMP_DEST:
			if (opts.alt_base_mode !=0 &&
			    opts.alt_base_mode != BASE_MODE_COMPARE) {
				errx(1, "option --%s conflicts with %s",
				    lopts[lidx].name,
				    alt_base_mode(opts.alt_base_mode));
			}
			opts.alt_base_mode = BASE_MODE_COMPARE;
#if 0
			goto basedir;
		case OP_COPY_DEST:
			if (opts.alt_base_mode !=0 &&
			    opts.alt_base_mode != BASE_MODE_COPY) {
				errx(1, "option --%s conflicts with %s",
				    lopts[lidx].name,
				    alt_base_mode(opts.alt_base_mode));
			}
			opts.alt_base_mode = BASE_MODE_COPY;
			goto basedir;
		case OP_LINK_DEST:
			if (opts.alt_base_mode !=0 &&
			    opts.alt_base_mode != BASE_MODE_LINK) {
				errx(1, "option --%s conflicts with %s",
				    lopts[lidx].name,
				    alt_base_mode(opts.alt_base_mode));
			}
			opts.alt_base_mode = BASE_MODE_LINK;

basedir:
#endif
			if (basedir_cnt >= MAX_BASEDIR)
				errx(1, "too many --%s directories specified",
				    lopts[lidx].name);
			opts.basedir[basedir_cnt++] = optarg;
			break;
		case OP_MAX_SIZE:
			if (scan_scaled(optarg, &opts.max_size) == -1)
				err(1, "bad max-size");
			break;
		case OP_MIN_SIZE:
			if (scan_scaled(optarg, &opts.min_size) == -1)
				err(1, "bad min-size");
			break;
		case OP_VERSION:
			fprintf(stderr, "openrsync: protocol version %u\n",
			    RSYNC_PROTOCOL);
			exit(0);
		case 'h':
		default:
			goto usage;
		}
	}

	argc -= optind;
	argv += optind;

	/* FIXME: reference implementation rsync accepts this. */

	if (argc < 2)
		goto usage;

	if (opts.port == NULL)
		opts.port = "rsync";

	/* by default and for --contimeout=0 disable poll_contimeout */
	if (poll_contimeout == 0)
		poll_contimeout = -1;
	else
		poll_contimeout *= 1000;

	/* by default and for --timeout=0 disable poll_timeout */
	if (poll_timeout == 0)
		poll_timeout = -1;
	else
		poll_timeout *= 1000;

	/*
	 * This is what happens when we're started with the "hidden"
	 * --server option, which is invoked for the rsync on the remote
	 * host by the parent.
	 */

	if (opts.server)
		exit(rsync_server(&opts, (size_t)argc, argv));

	/*
	 * Now we know that we're the client on the local machine
	 * invoking rsync(1).
	 * At this point, we need to start the client and server
	 * initiation logic.
	 * The client is what we continue running on this host; the
	 * server is what we'll use to connect to the remote and
	 * invoke rsync with the --server option.
	 */

	fargs = fargs_parse(argc, argv, &opts);
	assert(fargs != NULL);

	/*
	 * If we're contacting an rsync:// daemon, then we don't need to
	 * fork, because we won't start a server ourselves.
	 * Route directly into the socket code, unless a remote shell
	 * has explicitly been specified.
	 */

	if (fargs->remote && opts.ssh_prog == NULL) {
		assert(fargs->mode == FARGS_RECEIVER);
		if ((rc = rsync_connect(&opts, &sd, fargs)) == 0) {
			rc = rsync_socket(&opts, sd, fargs);
			close(sd);
		}
		exit(rc);
	}

	/* Drop the dns/inet possibility. */

	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw proc exec unveil",
	    NULL) == -1)
		err(ERR_IPC, "pledge");

	/* Create a bidirectional socket and start our child. */

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds) == -1)
		err(ERR_IPC, "socketpair");

	switch ((child = fork())) {
	case -1:
		err(ERR_IPC, "fork");
	case 0:
		close(fds[0]);
		if (pledge("stdio exec", NULL) == -1)
			err(ERR_IPC, "pledge");

		memset(&sess, 0, sizeof(struct sess));
		sess.opts = &opts;

		args = fargs_cmdline(&sess, fargs, NULL);

		for (i = 0; args[i] != NULL; i++)
			LOG2("exec[%d] = %s", i, args[i]);

		/* Make sure the child's stdin is from the sender. */
		if (dup2(fds[1], STDIN_FILENO) == -1)
			err(ERR_IPC, "dup2");
		if (dup2(fds[1], STDOUT_FILENO) == -1)
			err(ERR_IPC, "dup2");
		execvp(args[0], args);
		_exit(ERR_IPC);
		/* NOTREACHED */
	default:
		close(fds[1]);
		if (!fargs->remote)
			rc = rsync_client(&opts, fds[0], fargs);
		else
			rc = rsync_socket(&opts, fds[0], fargs);
		break;
	}

	close(fds[0]);

	if (waitpid(child, &st, 0) == -1)
		err(ERR_WAITPID, "waitpid");

	/*
	 * If we don't already have an error (rc == 0), then inherit the
	 * error code of rsync_server() if it has exited.
	 * If it hasn't exited, it overrides our return value.
	 */

	if (rc == 0) {
		if (WIFEXITED(st))
			rc = WEXITSTATUS(st);
		else if (WIFSIGNALED(st))
			rc = ERR_TERMIMATED;
		else
			rc = ERR_WAITPID;
	}

	exit(rc);
usage:
	fprintf(stderr, "usage: %s"
	    " [-aDglnoprtvx] [-e program] [--address=sourceaddr]\n"
	    "\t[--contimeout=seconds] [--compare-dest=dir] [--del] [--exclude]\n"
	    "\t[--exclude-from=file] [--include] [--include-from=file]\n"
	    "\t[--no-motd] [--numeric-ids] [--port=portnumber]\n"
	    "\t[--rsync-path=program] [--timeout=seconds] [--version]\n"
	    "\tsource ... directory\n",
	    getprogname());
	exit(ERR_SYNTAX);
}
