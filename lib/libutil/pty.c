/*	$OpenBSD: pty.c,v 1.15 2006/03/30 20:44:19 deraadt Exp $	*/

/*-
 * Copyright (c) 1990, 1993
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

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <grp.h>
#include <sys/tty.h>

#include "util.h"

#define TTY_LETTERS "pqrstuvwxyzPQRST"
#define TTY_SUFFIX "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

int
openpty(int *amaster, int *aslave, char *name, struct termios *termp,
    struct winsize *winp)
{
	char line[] = "/dev/ptyXX";
	const char *cp1, *cp2;
	int master, slave, fd;
	struct ptmget ptm;
	struct group *gr;
	gid_t ttygid;

	/*
	 * Try to use /dev/ptm and the PTMGET ioctl to get a properly set up
	 * and owned pty/tty pair. If this fails, (because we might not have
	 * the ptm device, etc.) fall back to using the traditional method
	 * of walking through the pty entries in /dev for the moment, until
	 * there is less chance of people being seriously boned by running
	 * kernels without /dev/ptm in them.
	 */
	fd = open(PATH_PTMDEV, O_RDWR, 0);
	if (fd == -1)
		goto walkit;
	if ((ioctl(fd, PTMGET, &ptm) == -1)) {
		close(fd);
		goto walkit;
	}
	close(fd);
	master = ptm.cfd;
	slave = ptm.sfd;
	if (name) {
		/*
		 * Manual page says "at least 16 characters".
		 */
		strlcpy(name, ptm.sn, 16);
	}
	*amaster = master;
	*aslave = slave;
	if (termp)
		(void) tcsetattr(slave, TCSAFLUSH, termp);
	if (winp)
		(void) ioctl(slave, TIOCSWINSZ, winp);
	return (0);
 walkit:
	if ((gr = getgrnam("tty")) != NULL)
		ttygid = gr->gr_gid;
	else
		ttygid = (gid_t)-1;

	for (cp1 = TTY_LETTERS; *cp1; cp1++) {
		line[8] = *cp1;
		for (cp2 = TTY_SUFFIX; *cp2; cp2++) {
			line[9] = *cp2;
			line[5] = 'p';
			if ((master = open(line, O_RDWR, 0)) == -1) {
				if (errno == ENOENT)
					return (-1);	/* out of ptys */
			} else {
				line[5] = 't';
				(void) chown(line, getuid(), ttygid);
				(void) chmod(line, S_IRUSR|S_IWUSR|S_IWGRP);
				(void) revoke(line);
				if ((slave = open(line, O_RDWR, 0)) != -1) {
					*amaster = master;
					*aslave = slave;
					if (name) {
						/*
						 * Manual page says "at least
						 * 16 characters".
						 */
						strlcpy(name, line, 16);
					}
					if (termp)
						(void) tcsetattr(slave,
						    TCSAFLUSH, termp);
					if (winp)
						(void) ioctl(slave, TIOCSWINSZ,
						    winp);
					return (0);
				}
				(void) close(master);
			}
		}
	}
	errno = ENOENT;	/* out of ptys */
	return (-1);
}

pid_t
forkpty(amaster, name, termp, winp)
	int *amaster;
	char *name;
	struct termios *termp;
	struct winsize *winp;
{
	int master, slave;
	pid_t pid;

	if (openpty(&master, &slave, name, termp, winp) == -1)
		return (-1);
	switch (pid = fork()) {
	case -1:
		return (-1);
	case 0:
		/*
		 * child
		 */
		(void) close(master);
		login_tty(slave);
		return (0);
	}
	/*
	 * parent
	 */
	*amaster = master;
	(void) close(slave);
	return (pid);
}
