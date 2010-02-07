/*	$OpenBSD: tipout.c,v 1.21 2010/02/07 20:16:47 nicm Exp $	*/
/*	$NetBSD: tipout.c,v 1.5 1996/12/29 10:34:12 cgd Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

#include <poll.h>

#include "tip.h"

/*
 * tip
 *
 * lower fork of tip -- handles passive side
 *  reading from the remote host
 */

static void	tipout_wait(void);
static void	tipout_script(void);
static void	tipout_write(char *, size_t);
static void	tipout_sighandler(int);

volatile sig_atomic_t tipout_die;

/*
 * TIPOUT wait state routine --
 *   sent by TIPIN when it wants to possess the remote host
 */
static void
tipout_wait(void)
{
	write(tipin_fd, &ccc, 1);
	read(tipin_fd, &ccc, 1);
}

/*
 * Scripting command interpreter --
 *  accepts script file name over the pipe and acts accordingly
 */
static void
tipout_script(void)
{
	char c, line[256];
	char *pline = line;
	char reply;

	read(tipin_fd, &c, 1);
	while (c != '\n' && pline - line < sizeof(line)) {
		*pline++ = c;
		read(tipin_fd, &c, 1);
	}
	*pline = '\0';
	if (boolean(value(SCRIPT)) && fscript != NULL)
		fclose(fscript);
	if (pline == line) {
		setboolean(value(SCRIPT), FALSE);
		reply = 'y';
	} else {
		if ((fscript = fopen(line, "a")) == NULL)
			reply = 'n';
		else {
			reply = 'y';
			setboolean(value(SCRIPT), TRUE);
		}
	}
	write(tipin_fd, &reply, 1);
}

/*
 * Write remote input out to stdout (and script file if enabled).
 */
static void
tipout_write(char *buf, size_t len)
{
	char *cp;

	for (cp = buf; cp < buf + len; cp++)
		*cp &= STRIP_PAR;

	write(STDOUT_FILENO, buf, len);

	if (boolean(value(SCRIPT)) && fscript != NULL) {
		if (!boolean(value(BEAUTIFY)))
			fwrite(buf, 1, len, fscript);
		else {
			for (cp = buf; cp < buf + len; cp++) {
				if ((*cp >= ' ' && *cp <= '~') ||
				    any(*cp, value(EXCEPTIONS)))
					putc(*cp, fscript);
			}
		}
	}
}

/* ARGSUSED */
static void
tipout_sighandler(int signo)
{
	tipout_die = 1;
}

/*
 * ****TIPOUT   TIPOUT****
 */
void
tipout(void)
{
	struct pollfd pfds[2];
	char buf[BUFSIZ], ch;
	ssize_t len;
	int flag;

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	tipout_die = 0;
	signal(SIGTERM, tipout_sighandler);
	signal(SIGHUP, tipout_sighandler);

	pfds[0].fd = tipin_fd;
	pfds[0].events = POLLIN;

	pfds[1].fd = FD;
	pfds[1].events = POLLIN;

	while (!tipout_die) {
		if (poll(pfds, 2, INFTIM) == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			goto fail;
		}

		if (pfds[0].revents & (POLLHUP|POLLERR))
			goto fail;
		if (pfds[0].revents & POLLIN) {
			switch (read(tipin_fd, &ch, 1)) {
			case 0:
				goto fail;
			case -1:
				if (errno == EINTR || errno == EAGAIN)
					break;
				goto fail;
			default:
				switch (ch) {
				case 'W':	/* wait state */
					tipout_wait();
					break;
				case 'S':	/* script file */
					tipout_script();
					break;
				case 'B':	/* toggle beautify */
					flag = !boolean(value(BEAUTIFY));
					setboolean(value(BEAUTIFY), flag);
					break;
				}
				break;
			}
		}

		if (pfds[1].revents & (POLLHUP|POLLERR))
			goto fail;
		if (pfds[1].revents & POLLIN) {
			switch (len = read(FD, buf, BUFSIZ)) {
			case 0:
				goto fail;
			case -1:
				if (errno == EINTR || errno == EAGAIN)
					continue;
				goto fail;
			default:
				tipout_write(buf, len);
				break;
			}
		}
	}

fail:
	if (boolean(value(SCRIPT)) && fscript != NULL)
		fclose(fscript);
	kill(tipin_pid, SIGTERM);
	exit(0);
}
