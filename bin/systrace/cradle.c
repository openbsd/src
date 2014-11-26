/*	$OpenBSD: cradle.c,v 1.8 2014/11/26 18:34:51 millert Exp $	*/

/*
 * Copyright (c) 2003 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <sys/tree.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#ifdef __linux__
#include <bits/posix1_lim.h>
#ifndef LOGIN_NAME_MAX
#define LOGIN_NAME_MAX _POSIX_LOGIN_NAME_MAX
#endif
#endif /* __linux__ */

#include "intercept.h"
#include "systrace.h"

extern int connected;
extern char dirpath[];

static struct event listen_ev;
static struct event uilisten_ev;

static int   cradle_server(char *, char *, char *);
static void  listen_cb(int, short, void *);
static void  msg_cb(int, short, void *);
static void  ui_cb(int, short, void *);
static void  gensig_cb(int, short, void *);

static FILE *ui_fl = NULL;
static struct event ui_ev, sigterm_ev, sigint_ev;
static char buffer[4096];
static char *xuipath, *xpath;
static volatile int got_sigusr1 = 0;

struct client {
	struct event         ev;
	FILE                *fl;
	int                  buffered;
	TAILQ_ENTRY(client)  next;
};

TAILQ_HEAD(client_head, client) clientq;

/* fake signal handler */
/* ARGSUSED */
static void
sigusr1_handler(int sig)
{
	got_sigusr1 = 1;
}

/* ARGSUSED */
static void
gensig_cb(int sig, short ev, void *data)
{
	unlink(xpath);
	unlink(xuipath);

	rmdir(dirpath);

	exit(1);
}

static int
mkunserv(char *path)
{
	int s;
	mode_t old_umask;
	struct sockaddr_un sun;

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket()");

	memset(&sun, 0, sizeof (sun));
	sun.sun_family = AF_UNIX;

	if (strlcpy(sun.sun_path, path, sizeof (sun.sun_path)) >=
	    sizeof (sun.sun_path))
		errx(1, "Path too long: %s", path);

	old_umask = umask(S_IRUSR | S_IWUSR);
	if (bind(s, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "bind()");
	umask(old_umask);

	if (chmod(path, S_IRUSR | S_IWUSR) == -1)
		err(1, "chmod()");

	if (listen(s, 10) == -1)
		err(1, "listen()");

	return (s);
}

static int
cradle_server(char *path, char *uipath, char *guipath)
{
	int s, uis;
	pid_t pid, newpid;
	sigset_t none, set, oldset;
	sig_t oldhandler;

	sigemptyset(&none);
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	if (sigprocmask(SIG_BLOCK, &set, &oldset) == -1)
		err(1, "sigprocmask()");
	oldhandler = signal(SIGUSR1, sigusr1_handler);
	if (oldhandler == SIG_ERR)
		err(1, "signal()");

	xpath = path;
	xuipath = uipath;

	pid = getpid();
	newpid = fork();

	switch (newpid) {
	case -1:
		err(1, "fork()");
	case 0:
		break;
	default:
		/*
		 * Parent goes to sleep waiting for server to start.
		 * When it wakes up, we can start the GUI.
		 */
		sigsuspend(&none);
		if (signal(SIGUSR1, oldhandler) == SIG_ERR)
			err(1, "signal()");
		if (sigprocmask(SIG_SETMASK, &oldset, NULL) == -1)
			err(1, "sigprocmask()");
		if (got_sigusr1) {
			requestor_start(guipath, 1);
			return (0);
		} else
			return (-1);
	}

	setsid();
	setproctitle("cradle server for UID %d", getuid());

	TAILQ_INIT(&clientq);

	event_init();

	s = mkunserv(path);
	uis = mkunserv(uipath);

	signal_set(&sigterm_ev, SIGTERM, gensig_cb, NULL);
	if (signal_add(&sigterm_ev, NULL) == -1)
		err(1, "signal_add()");

	signal_set(&sigint_ev, SIGINT, gensig_cb, NULL);
	if (signal_add(&sigint_ev, NULL) == -1)
		err(1, "signal_add()");

	event_set(&listen_ev, s, EV_READ, listen_cb, NULL);
	if (event_add(&listen_ev, NULL) == -1)
		err(1, "event_add()");

	event_set(&uilisten_ev, uis, EV_READ, listen_cb, &listen_cb);
	if (event_add(&uilisten_ev, NULL) == -1)
		err(1, "event_add()");

	kill(pid, SIGUSR1);

	event_dispatch();
	errx(1, "event_dispatch()");
	/* NOTREACHED */
	/* gcc fodder */
	return (-1);
}

void
cradle_start(char *path, char *uipath, char *guipath)
{
	int s;
	struct sockaddr_un sun;

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s == -1)
		err(1, "socket()");

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;

	if (strlcpy(sun.sun_path, path, sizeof (sun.sun_path)) >=
	    sizeof (sun.sun_path))
		errx(1, "Path too long: %s", path);

	while (connect(s, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		if (errno != ENOENT)
			err(1, "connect()");

		if (cradle_server(path, uipath, guipath) == -1)
			errx(1, "failed contacting or starting cradle server");
	}

	if (dup2(s, fileno(stdin)) == -1)
		err(1, "dup2");
	if (dup2(s, fileno(stdout)) == -1)
		err(1, "dup2");
	setvbuf(stdout, NULL, _IOLBF, 0);

	connected = 1;
}

/* ARGSUSED */
static void
listen_cb(int fd, short which, void *arg)
{
	int s, ui = arg != NULL;
	struct sockaddr sa;
	struct client *cli;
	socklen_t salen = sizeof(sa);
	struct event *ev;

	s = accept(fd, &sa, &salen);
	if (s == -1) {
		if (errno != EINTR && errno != EWOULDBLOCK &&
		    errno != ECONNABORTED)
			warn("accept()");
		goto out;
	}

	if (ui) {
		if (ui_fl != NULL)
			goto out;

		if ((ui_fl = fdopen(s, "w+")) == NULL)
			err(1, "fdopen()");
		setvbuf(ui_fl, NULL, _IONBF, 0);
		event_set(&ui_ev, s, EV_READ | EV_PERSIST, ui_cb, NULL);

		/* Dequeue UI-pending events */
		while ((cli = TAILQ_FIRST(&clientq)) != NULL) {
			TAILQ_REMOVE(&clientq, cli, next);
			msg_cb(fileno(cli->fl), EV_READ, cli);
			if (ui_fl == NULL)
				break;
		}

		if (event_add(&ui_ev, NULL) == -1)
			err(1, "event_add()");
	} else {
		if ((cli = calloc(1, sizeof(*cli))) == NULL)
			err(1, "calloc()");

		if ((cli->fl = fdopen(s, "w+")) == NULL)
			err(1, "fdopen()");

		setvbuf(cli->fl, NULL, _IONBF, 0);
		event_set(&cli->ev, s, EV_READ, msg_cb, cli);
		if (event_add(&cli->ev, NULL) == -1)
			err(1, "event_add()");
	}
 out:
	ev = ui ? &uilisten_ev : &listen_ev;
	if (event_add(ev, NULL) == -1)
		err(1, "event_add()");
}

/* ARGSUSED */
static void
msg_cb(int fd, short which, void *arg)
{
	struct client *cli = arg;
	char line[4096];

	if (ui_fl == NULL) {
		TAILQ_INSERT_TAIL(&clientq, cli, next);
		return;
	}

	/* Policy question from systrace */
	if (!cli->buffered)
		if (fgets(buffer, sizeof(buffer), cli->fl) == NULL)
			goto out_eof;
	cli->buffered = 0;

	if (fputs(buffer, ui_fl) == EOF)
		goto out_uieof0;
 again_answer:
	/* Policy answer from UI */
	if (fgets(line, sizeof(line), ui_fl) == NULL)
		goto out_uieof0;
	if (fputs(line, cli->fl) == EOF)
		goto out_eof;
	/* Status from systrace */
	while (1) {
		if (fgets(line, sizeof(line), cli->fl) == NULL)
			goto out_eof;
		if (fputs(line, ui_fl) == EOF)
			goto out_uieof1;
		if (strcmp(line, "WRONG\n") == 0)
			goto again_answer;
		if (strcmp(line, "OKAY\n") == 0)
			break;
	}

 out_event:
	if (event_add(&cli->ev, NULL) == -1)
		err(1, "event_add()");
	return;

 out_eof:
	fclose(cli->fl);
	free(cli);
	return;

 out_uieof0:
	fclose(ui_fl);
	ui_fl = NULL;
	cli->buffered = 1;
	TAILQ_INSERT_HEAD(&clientq, cli, next);
	return;

 out_uieof1:
	fclose(ui_fl);
	ui_fl = NULL;
	while (1) {
		/* We have a line coming in... */
		if (strcmp(line, "WRONG\n") == 0)
			if (fputs("kill\n", cli->fl) == EOF)
				goto out_eof;
		if (strcmp(line, "OKAY\n") == 0)
			break;
		if (fgets(line, sizeof(line), cli->fl) == NULL)
			goto out_eof;
	}

	goto out_event;
}

/*
 * Hack to catch "idle" EOFs from the UI
 */
/* ARGSUSED */
static void
ui_cb(int fd, short which, void *arg)
{
	char c;

	fread(&c, sizeof(c), 1, ui_fl);

	if (feof(ui_fl)) {
		ui_fl = NULL;
		event_del(&ui_ev);
	} else
		warnx("Junk from UI");
}
