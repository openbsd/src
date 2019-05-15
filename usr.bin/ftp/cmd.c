/*	$OpenBSD: cmd.c,v 1.3 2019/05/15 13:42:40 florian Exp $ */

/*
 * Copyright (c) 2018 Sunil Nimmagadda <sunil@openbsd.org>
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

#include <sys/socket.h>
#include <sys/stat.h>

#include <arpa/telnet.h>

#include <err.h>
#include <errno.h>
#include <histedit.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftp.h"

#define ARGVMAX	64

static void	 cmd_interrupt(int);
static int	 cmd_lookup(const char *);
static FILE	*data_fopen(const char *);
static void	 do_open(int, char **);
static void	 do_help(int, char **);
static void	 do_quit(int, char **);
static void	 do_ls(int, char **);
static void	 do_pwd(int, char **);
static void	 do_cd(int, char **);
static void	 do_get(int, char **);
static void	 do_passive(int, char **);
static void	 do_lcd(int, char **);
static void	 do_lpwd(int, char **);
static void	 do_put(int, char **);
static void	 do_mget(int, char **);
static void	 ftp_abort(void);
static char	*prompt(void);

static FILE	*ctrl_fp, *data_fp;

static struct {
	const char	 *name;
	const char	 *info;
	void		(*cmd)(int, char **);
	int		  conn_required;
} cmd_tbl[] = {
	{ "open", "connect to remote ftp server", do_open, 0 },
	{ "close", "terminate ftp session", do_quit, 1 },
	{ "help", "print local help information", do_help, 0 },
	{ "?", "print local help information", do_help, 0 },
	{ "quit", "terminate ftp session and exit", do_quit, 0 },
	{ "exit", "terminate ftp session and exit", do_quit, 0 },
	{ "ls", "list contents of remote directory", do_ls, 1 },
	{ "pwd", "print working directory on remote machine", do_pwd, 1 },
	{ "cd", "change remote working directory", do_cd, 1 },
	{ "nlist", "nlist contents of remote directory", do_ls, 1 },
	{ "get", "receive file", do_get, 1 },
	{ "passive", "toggle passive transfer mode", do_passive, 0 },
	{ "lcd", "change local working directory", do_lcd, 0 },
	{ "lpwd", "print local working directory", do_lpwd, 0 },
	{ "put", "send one file", do_put, 1 },
	{ "mget", "get multiple files", do_mget, 1 },
	{ "mput", "send multiple files", do_mget, 1 },
};

static void
cmd_interrupt(int signo)
{
	const char	msg[] = "\rwaiting for remote to finish abort\n";
	int		save_errno = errno;

	if (data_fp != NULL)
		(void)write(STDERR_FILENO, msg, sizeof(msg) - 1);

	interrupted = 1;
	errno = save_errno;
}

void
cmd(const char *host, const char *port, const char *path)
{
	HistEvent	  hev;
	EditLine	 *el;
	History		 *hist;
	const char	 *line;
	char		**ap, *argv[ARGVMAX], *cp;
	int		  count, i;

	if ((el = el_init(getprogname(), stdin, stdout, stderr)) == NULL)
		err(1, "couldn't initialise editline");

	if ((hist = history_init()) == NULL)
		err(1, "couldn't initialise editline history");

	history(hist, &hev, H_SETSIZE, 100);
	el_set(el, EL_HIST, history, hist);
	el_set(el, EL_PROMPT, prompt);
	el_set(el, EL_EDITOR, "emacs");
	el_set(el, EL_TERMINAL, NULL);
	el_set(el, EL_SIGNAL, 1);
	el_source(el, NULL);

	if (host != NULL) {
		argv[0] = "open";
		argv[1] = (char *)host;
		argv[2] = port ? (char *)port : "21";
		do_open(3, argv);
		/* If we don't have a connection, exit */
		if (ctrl_fp == NULL)
			exit(1);

		if (path != NULL) {
			argv[0] = "cd";
			argv[1] = (char *)path;
			do_cd(2, argv);
		}
	}

	for (;;) {
		signal(SIGINT, SIG_IGN);
		if ((line = el_gets(el, &count)) == NULL || count <= 0) {
			if (verbose)
				fprintf(stderr, "\n");
			argv[0] = "quit";
			do_quit(1, argv);
			break;
		}

		if (count <= 1)
			continue;

		if ((cp = strrchr(line, '\n')) != NULL)
			*cp = '\0';

		history(hist, &hev, H_ENTER, line);
		for (ap = argv; ap < &argv[ARGVMAX - 1] &&
		    (*ap = strsep((char **)&line, " \t")) != NULL;) {
			if (**ap != '\0')
				ap++;
		}
		*ap = NULL;

		if (argv[0] == NULL)
			continue;

		if ((i = cmd_lookup(argv[0])) == -1) {
			fprintf(stderr, "Invalid command.\n");
			continue;
		}

		if (cmd_tbl[i].conn_required && ctrl_fp == NULL) {
			fprintf(stderr, "Not connected.\n");
			continue;
		}

		interrupted = 0;
		signal(SIGINT, cmd_interrupt);
		cmd_tbl[i].cmd(ap - argv, argv);

		if (strcmp(cmd_tbl[i].name, "quit") == 0 ||
		    strcmp(cmd_tbl[i].name, "exit") == 0)
			break;
	}

	el_end(el);
}

static int
cmd_lookup(const char *cmd)
{
	size_t	i;

	for (i = 0; i < nitems(cmd_tbl); i++)
		if (strcmp(cmd, cmd_tbl[i].name) == 0)
			return i;

	return -1;
}

static char *
prompt(void)
{
	return "ftp> ";
}

static FILE *
data_fopen(const char *mode)
{
	int	 fd;

	fd = activemode ? ftp_eprt(ctrl_fp) : ftp_epsv(ctrl_fp);
	if (fd == -1) {
		if (io_debug)
			fprintf(stderr, "Failed to open data connection");

		return NULL;
	}

	return fdopen(fd, mode);
}

static void
ftp_abort(void)
{
	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "%c%c%c", IAC, IP, IAC);
	if (send(fileno(ctrl_fp), buf, 3, MSG_OOB) != 3)
		warn("abort");

	ftp_command(ctrl_fp, "%cABOR", DM);
}

static void
do_open(int argc, char **argv)
{
	const char	*host = NULL, *port = "21";
	char		*buf = NULL;
	size_t		 n = 0;
	int		 sock;

	if (ctrl_fp != NULL) {
		fprintf(stderr, "already connected, use close first.\n");
		return;
	}

	switch (argc) {
	case 3:
		port = argv[2];
		/* FALLTHROUGH */
	case 2:
		host = argv[1];
		break;
	default:
		fprintf(stderr, "usage: open host [port]\n");
		return;
	}

	if ((sock = tcp_connect(host, port, 0)) == -1)
		return;

	fprintf(stderr, "Connected to %s.\n", host);
	if ((ctrl_fp = fdopen(sock, "r+")) == NULL)
		err(1, "%s: fdopen", __func__);

	/* greeting */
	ftp_getline(&buf, &n, 0, ctrl_fp);
	free(buf);
	if (ftp_auth(ctrl_fp, NULL, NULL) != P_OK) {
		fclose(ctrl_fp);
		ctrl_fp = NULL;
	}
}

static void
do_help(int argc, char **argv)
{
	size_t	i;
	int	j;

	if (argc == 1) {
		for (i = 0; i < nitems(cmd_tbl); i++)
			fprintf(stderr, "%s\n", cmd_tbl[i].name);

		return;
	}

	for (i = 1; i < (size_t)argc; i++) {
		if ((j = cmd_lookup(argv[i])) == -1)
			fprintf(stderr, "invalid help command %s\n", argv[i]);
		else
			fprintf(stderr, "%s\t%s\n", argv[i], cmd_tbl[j].info);
	}
}

static void
do_quit(int argc, char **argv)
{
	if (ctrl_fp == NULL)
		return;

	ftp_command(ctrl_fp, "QUIT");
	fclose(ctrl_fp);
	ctrl_fp = NULL;
}

static void
do_ls(int argc, char **argv)
{
	FILE		*dst_fp = stdout;
	const char	*cmd, *local_fname = NULL, *remote_dir = NULL;
	char		*buf = NULL;
	size_t		 n = 0;
	ssize_t		 len;
	int		 r;

	switch (argc) {
	case 3:
		if (strcmp(argv[2], "-") != 0)
			local_fname = argv[2];
		/* FALLTHROUGH */
	case 2:
		remote_dir = argv[1];
		/* FALLTHROUGH */
	case 1:
		break;
	default:
		fprintf(stderr, "usage: ls [remote-directory [local-file]]\n");
		return;
	}

	if ((data_fp = data_fopen("r")) == NULL)
		return;

	if (local_fname && (dst_fp = fopen(local_fname, "w")) == NULL) {
		warn("fopen %s", local_fname);
		fclose(data_fp);
		data_fp = NULL;
		return;
	}

	cmd = (strcmp(argv[0], "ls") == 0) ? "LIST" : "NLST";
	if (remote_dir != NULL)
		r = ftp_command(ctrl_fp, "%s %s", cmd, remote_dir);
	else
		r = ftp_command(ctrl_fp, "%s", cmd);

	if (r != P_PRE) {
		fclose(data_fp);
		data_fp = NULL;
		if (dst_fp != stdout)
			fclose(dst_fp);

		return;
	}

	while ((len = getline(&buf, &n, data_fp)) != -1 && !interrupted) {
		buf[len - 1] = '\0';
		if (len >= 2 && buf[len - 2] == '\r')
			buf[len - 2] = '\0';

		fprintf(dst_fp, "%s\n", buf);
	}

	if (interrupted)
		ftp_abort();

	fclose(data_fp);
	data_fp = NULL;
	ftp_getline(&buf, &n, 0, ctrl_fp);
	free(buf);
	if (dst_fp != stdout)
		fclose(dst_fp);
}

static void
do_get(int argc, char **argv)
{
	FILE		*dst_fp;
	const char	*local_fname = NULL, *p, *remote_fname;
	char		*buf = NULL;
	size_t		 n = 0;
	off_t		 file_sz, offset = 0;

	switch (argc) {
	case 3:
		local_fname = argv[2];
		/* FALLTHROUGH */
	case 2:
		remote_fname = argv[1];
		break;
	default:
		fprintf(stderr, "usage: get remote-file [local-file]\n");
		return;
	}

	if (local_fname == NULL)
		local_fname = remote_fname;

	if (ftp_command(ctrl_fp, "TYPE I") != P_OK)
		return;

	log_info("local: %s remote: %s\n", local_fname, remote_fname);
	if (ftp_size(ctrl_fp, remote_fname, &file_sz, &buf) != P_OK) {
		fprintf(stderr, "%s", buf);
		return;
	}

	if ((data_fp = data_fopen("r")) == NULL)
		return;

	if ((dst_fp = fopen(local_fname, "w")) == NULL) {
		warn("%s", local_fname);
		fclose(data_fp);
		data_fp = NULL;
		return;
	}

	if (ftp_command(ctrl_fp, "RETR %s", remote_fname) != P_PRE) {
		fclose(data_fp);
		data_fp = NULL;
		fclose(dst_fp);
		return;
	}

	init_stats(file_sz, &offset);
	if (progressmeter) {
		p = basename(remote_fname);
		start_progress_meter(p, NULL);
	}

	copy_file(dst_fp, data_fp, &offset);
	if (progressmeter)
		stop_progress_meter();
	finish_stats();

	if (interrupted)
		ftp_abort();

	fclose(data_fp);
	data_fp = NULL;
	fclose(dst_fp);
	ftp_getline(&buf, &n, 0, ctrl_fp);
	free(buf);
}

static void
do_pwd(int argc, char **argv)
{
	ftp_command(ctrl_fp, "PWD");
}

static void
do_cd(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: cd remote-directory\n");
		return;
	}

	ftp_command(ctrl_fp, "CWD %s", argv[1]);
}

static void
do_passive(int argc, char **argv)
{
	switch (argc) {
	case 1:
		break;
	case 2:
		if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "off") == 0)
			break;

		/* FALLTHROUGH */
	default:
		fprintf(stderr, "usage: passive [on | off]\n");
		return;
	}

	if (argv[1] != NULL) {
		activemode = (strcmp(argv[1], "off") == 0) ? 1 : 0;
		fprintf(stderr, "passive mode is %s\n", argv[1]);
		return;
	}

	activemode = !activemode;
	fprintf(stderr, "passive mode is %s\n", activemode ? "off" : "on");
}

static void
do_lcd(int argc, char **argv)
{
	struct passwd	*pw = NULL;
	const char	*dir, *login;
	char		 cwd[PATH_MAX];

	switch (argc) {
	case 1:
	case 2:
		break;
	default:
		fprintf(stderr, "usage: lcd [local-directory]\n");
		return;
	}

	if ((login = getlogin()) != NULL)
		pw = getpwnam(login);

	if (pw == NULL && (pw = getpwuid(getuid())) == NULL) {
		fprintf(stderr, "Failed to get home directory\n");
		return;
	}

	dir = argv[1] ? argv[1] : pw->pw_dir;
	if (chdir(dir) != 0) {
		warn("local: %s", dir);
		return;
	}

	if (getcwd(cwd, sizeof cwd) == NULL) {
		warn("getcwd");
		return;
	}

	fprintf(stderr, "Local directory now %s\n", cwd);
}

static void
do_lpwd(int argc, char **argv)
{
	char	cwd[PATH_MAX];

	if (getcwd(cwd, sizeof cwd) == NULL) {
		warn("getcwd");
		return;
	}

	fprintf(stderr, "Local directory %s\n", cwd);
}

static void
do_put(int argc, char **argv)
{
	struct stat	 sb;
	FILE		*src_fp;
	const char	*local_fname, *p, *remote_fname = NULL;
	char		*buf = NULL;
	size_t		 n = 0;
	off_t		 file_sz, offset = 0;

	switch (argc) {
	case 3:
		remote_fname = argv[2];
		/* FALLTHROUGH */
	case 2:
		local_fname = argv[1];
		break;
	default:
		fprintf(stderr, "usage: put local-file [remote-file]\n");
		return;
	}

	if (remote_fname == NULL)
		remote_fname = local_fname;

	if (ftp_command(ctrl_fp, "TYPE I") != P_OK)
		return;

	log_info("local: %s remote: %s\n", local_fname, remote_fname);
	if ((data_fp = data_fopen("w")) == NULL)
		return;

	if ((src_fp = fopen(local_fname, "r")) == NULL) {
		warn("%s", local_fname);
		fclose(data_fp);
		data_fp = NULL;
		return;
	}

	if (fstat(fileno(src_fp), &sb) != 0) {
		warn("%s", local_fname);
		fclose(data_fp);
		data_fp = NULL;
		fclose(src_fp);
		return;
	}
	file_sz = sb.st_size;

	if (ftp_command(ctrl_fp, "STOR %s", remote_fname) != P_PRE) {
		fclose(data_fp);
		data_fp = NULL;
		fclose(src_fp);
		return;
	}

	init_stats(file_sz, &offset);
	if (progressmeter) {
		p = basename(remote_fname);
		start_progress_meter(p, NULL);
	}

	copy_file(data_fp, src_fp, &offset);
	if (progressmeter)
		stop_progress_meter();
	finish_stats();

	if (interrupted)
		ftp_abort();

	fclose(data_fp);
	data_fp = NULL;
	fclose(src_fp);
	ftp_getline(&buf, &n, 0, ctrl_fp);
	free(buf);
}

static void
do_mget(int argc, char **argv)
{
	void		(*fn)(int, char **);
	const char	 *usage;
	char		 *args[2];
	int		  i;

	if (strcmp(argv[0], "mget") == 0) {
		fn = do_get;
		args[0] = "get";
		usage = "mget remote-files";
	} else {
		fn = do_put;
		args[0] = "put";
		usage = "mput local-files";
	}

	if (argc == 1) {
		fprintf(stderr, "usage: %s\n", usage);
		return;
	}

	for (i = 1; i < argc && !interrupted; i++) {
		args[1] = argv[i];
		fn(2, args);
	}
}
