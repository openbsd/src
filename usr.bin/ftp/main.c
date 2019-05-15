/*	$OpenBSD: main.c,v 1.128 2019/05/15 13:42:40 florian Exp $ */

/*
 * Copyright (c) 2015 Sunil Nimmagadda <sunil@openbsd.org>
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

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <imsg.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftp.h"
#include "xmalloc.h"

static int		 auto_fetch(int, char **);
static void		 child(int, int, char **);
static int		 parent(int, pid_t);
static struct url	*proxy_parse(const char *);
static struct url	*get_proxy(int);
static void		 re_exec(int, int, char **);
static void		 validate_output_fname(struct url *, const char *);
static __dead void	 usage(void);

struct imsgbuf		 child_ibuf;
const char		*useragent = "OpenBSD ftp";
int			 activemode, family = AF_UNSPEC, io_debug;
int			 progressmeter, verbose = 1;
volatile sig_atomic_t	 interrupted = 0;
FILE			*msgout = stdout;

static const char	*title;
static char		*tls_options, *oarg;
static int		 connect_timeout, resume;

int
main(int argc, char **argv)
{
	const char	 *e;
	char		**save_argv, *term;
	int		  ch, csock, dumb_terminal, rexec, save_argc;

	if (isatty(fileno(stdin)) != 1)
		verbose = 0;

	io_debug = getenv("IO_DEBUG") != NULL;
	term = getenv("TERM");
	dumb_terminal = (term == NULL || *term == '\0' ||
	    !strcmp(term, "dumb") || !strcmp(term, "emacs") ||
	    !strcmp(term, "su"));
	if (isatty(STDOUT_FILENO) && isatty(STDERR_FILENO) && !dumb_terminal)
		progressmeter = 1;

	csock = rexec = 0;
	save_argc = argc;
	save_argv = argv;
	while ((ch = getopt(argc, argv,
	    "46AaCc:dD:Eegik:Mmno:pP:r:S:s:tU:vVw:xz:")) != -1) {
		switch (ch) {
		case '4':
			family = AF_INET;
			break;
		case '6':
			family = AF_INET6;
			break;
		case 'A':
			activemode = 1;
			break;
		case 'C':
			resume = 1;
			break;
		case 'D':
			title = optarg;
			break;
		case 'o':
			oarg = optarg;
			if (!strlen(oarg))
				oarg = NULL;
			break;
		case 'M':
			progressmeter = 0;
			break;
		case 'm':
			progressmeter = 1;
			break;
		case 'S':
			tls_options = optarg;
			break;
		case 'U':
			useragent = optarg;
			break;
		case 'V':
			verbose = 0;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			connect_timeout = strtonum(optarg, 0, 200, &e);
			if (e)
				errx(1, "-w: %s", e);
			break;
		/* options for internal use only */
		case 'x':
			rexec = 1;
			break;
		case 'z':
			csock = strtonum(optarg, 3, getdtablesize() - 1, &e);
			if (e)
				errx(1, "-z: %s", e);
			break;
		/* Ignoring all remaining options */
		case 'a':
		case 'c':
		case 'd':
		case 'E':
		case 'e':
		case 'g':
		case 'i':
		case 'k':
		case 'n':
		case 'P':
		case 'p':
		case 'r':
		case 's':
		case 't':
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (rexec)
		child(csock, argc, argv);

#ifndef SMALL
	struct url	*url;

	switch (argc) {
	case 0:
		cmd(NULL, NULL, NULL);
		return 0;
	case 1:
	case 2:
		switch (scheme_lookup(argv[0])) {
		case -1:
			cmd(argv[0], argv[1], NULL);
			return 0;
		case S_FTP:
			if ((url = url_parse(argv[0])) == NULL)
				exit(1);

			if (url->path &&
			    url->path[strlen(url->path) - 1] != '/')
				break; /* auto fetch */

			cmd(url->host, url->port, url->path);
			return 0;
		}
		break;
	}
#else
	if (argc == 0)
		usage();
#endif

	return auto_fetch(save_argc, save_argv);
}

static int
auto_fetch(int sargc, char **sargv)
{
	pid_t	pid;
	int	sp[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sp) != 0)
		err(1, "socketpair");

	switch (pid = fork()) {
	case -1:
		err(1, "fork");
	case 0:
		close(sp[0]);
		re_exec(sp[1], sargc, sargv);
	}

	close(sp[1]);
	return parent(sp[0], pid);
}

static void
re_exec(int sock, int argc, char **argv)
{
	char	**nargv, *sock_str;
	int	  i, j, nargc;

	nargc = argc + 4;
	nargv = xcalloc(nargc, sizeof(*nargv));
	xasprintf(&sock_str, "%d", sock);
	i = 0;
	nargv[i++] = argv[0];
	nargv[i++] = "-z";
	nargv[i++] = sock_str;
	nargv[i++] = "-x";
	for (j = 1; j < argc; j++)
		nargv[i++] = argv[j];

	execvp(nargv[0], nargv);
	err(1, "execvp");
}

static int
parent(int sock, pid_t child_pid)
{
	struct imsgbuf	ibuf;
	struct imsg	imsg;
	struct stat	sb;
	off_t		offset;
	int		fd, save_errno, sig, status;

	setproctitle("%s", "parent");
	if (pledge("stdio cpath rpath wpath sendfd", NULL) == -1)
		err(1, "pledge");

	imsg_init(&ibuf, sock);
	for (;;) {
		if (read_message(&ibuf, &imsg) == 0)
			break;

		if (imsg.hdr.type != IMSG_OPEN)
			errx(1, "%s: IMSG_OPEN expected", __func__);

		offset = 0;
		fd = open(imsg.data, imsg.hdr.peerid, 0666);
		save_errno = errno;
		if (fd != -1 && fstat(fd, &sb) == 0) {
			if (sb.st_mode & S_IFDIR) {
				close(fd);
				fd = -1;
				save_errno = EISDIR;
			} else
				offset = sb.st_size;
		}

		send_message(&ibuf, IMSG_OPEN, save_errno,
		    &offset, sizeof offset, fd);
		imsg_free(&imsg);
	}

	close(sock);
	if (waitpid(child_pid, &status, 0) == -1 && errno != ECHILD)
		err(1, "wait");

	sig = WTERMSIG(status);
	if (WIFSIGNALED(status) && sig != SIGPIPE)
		errx(1, "child terminated: signal %d", sig);

	return WEXITSTATUS(status);
}

static void
child(int sock, int argc, char **argv)
{
	struct url	*url;
	FILE		*dst_fp;
	char		*p;
	off_t		 offset, sz;
	int		 fd, i, tostdout;

	setproctitle("%s", "child");
#ifndef NOSSL
	https_init(tls_options);
#endif
	if (pledge("stdio inet dns recvfd tty", NULL) == -1)
		err(1, "pledge");
	if (!progressmeter && pledge("stdio inet dns recvfd", NULL) == -1)
		err(1, "pledge");

	imsg_init(&child_ibuf, sock);
	tostdout = oarg && (strcmp(oarg, "-") == 0);
	if (tostdout)
		msgout = stderr;
	if (resume && tostdout)
		errx(1, "can't append to stdout");

	for (i = 0; i < argc; i++) {
		fd = -1;
		offset = sz = 0;

		if ((url = url_parse(argv[i])) == NULL)
			exit(1);

		validate_output_fname(url, argv[i]);
		url_connect(url, get_proxy(url->scheme), connect_timeout);
		if (resume)
			fd = fd_request(url->fname, O_WRONLY|O_APPEND, &offset);

		url = url_request(url, get_proxy(url->scheme), &offset, &sz);
		if (resume && offset == 0 && fd != -1)
			if (ftruncate(fd, 0) != 0)
				err(1, "ftruncate");

		if (fd == -1 && !tostdout &&
		    (fd = fd_request(url->fname,
		    O_CREAT|O_TRUNC|O_WRONLY, NULL)) == -1)
			err(1, "Can't open file %s", url->fname);

		if (tostdout) {
			dst_fp = stdout;
		} else if ((dst_fp = fdopen(fd, "w")) == NULL)
			err(1, "%s: fdopen", __func__);

		init_stats(sz, &offset);
		if (progressmeter) {
			p = basename(url->path);
			start_progress_meter(p, title);
		}

		url_save(url, dst_fp, &offset);
		if (progressmeter)
			stop_progress_meter();
		finish_stats();

		if (!tostdout)
			fclose(dst_fp);

		url_close(url);
		url_free(url);
	}

	exit(0);
}

static struct url *
get_proxy(int scheme)
{
	static struct url	*ftp_proxy, *http_proxy;

	switch (scheme) {
	case S_HTTP:
	case S_HTTPS:
		if (http_proxy)
			return http_proxy;
		else
			return (http_proxy = proxy_parse("http_proxy"));
	case S_FTP:
		if (ftp_proxy)
			return ftp_proxy;
		else
			return (ftp_proxy = proxy_parse("ftp_proxy"));
	default:
		return NULL;
	}
}

static void
validate_output_fname(struct url *url, const char *name)
{
	url->fname = xstrdup(oarg ? oarg : basename(url->path));
	if (strcmp(url->fname, "/") == 0)
		errx(1, "No filename after host (use -o): %s", name);

	if (strcmp(url->fname, ".") == 0)
		errx(1, "No '/' after host (use -o): %s", name);
}

static struct url *
proxy_parse(const char *name)
{
	struct url	*proxy;
	char		*str;

	if ((str = getenv(name)) == NULL)
		return NULL;

	if (strlen(str) == 0)
		return NULL;

	if ((proxy = url_parse(str)) == NULL)
		exit(1);

	if (proxy->scheme != S_HTTP)
		errx(1, "Malformed proxy URL: %s", str);

	return proxy;
}

static __dead void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-46AVv] [-D title] [host [port]]\n"
	    "\t%s [-46ACMmVv] [-D title] [-o output] [-S tls_options]\n"
	    "\t\t[-U useragent] [-w seconds] url ...\n", getprogname(),
	     getprogname());

	exit(1);
}
