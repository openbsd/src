/*	$OpenBSD: termios.c,v 1.2 2005/05/24 20:38:20 uwe Exp $	*/

/*-
 * Copyright (c) 1989, 1993
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

#include "libsa.h"

/* Linux-specific line speed handling from linux_termios.c */

static speed_t linux_speeds[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 230400
};

static const int linux_spmasks[] = {
	LINUX_B0, LINUX_B50, LINUX_B75, LINUX_B110, LINUX_B134, LINUX_B150,
	LINUX_B200, LINUX_B300, LINUX_B600, LINUX_B1200, LINUX_B1800,
	LINUX_B2400, LINUX_B4800, LINUX_B9600, LINUX_B19200, LINUX_B38400,
	LINUX_B57600, LINUX_B115200, LINUX_B230400
};

int
cfsetspeed(struct termios *t, speed_t speed)
{
	int mask;
	int i;

	mask = LINUX_B9600;	/* XXX default value should this be 0? */
	for (i = 0; i < sizeof (linux_speeds) / sizeof (speed_t); i++) {
		if (speed == linux_speeds[i]) {
			mask = linux_spmasks[i];
			break;
		}
	}
	t->c_cflag &= ~LINUX_CBAUD;
	t->c_cflag |= mask;

	return (0);
}

void
cfmakeraw(struct termios *t)
{
	t->c_iflag &= ~(IMAXBEL|IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8;
}

int
tcgetattr(int fd, struct termios *t)
{
	return (uioctl(fd, TIOCGETA, t));
}

/* This function differs slightly from tcsetattr() in libc. */
int
tcsetattr(int fd, int action, struct termios *t)
{
	switch (action) {
	case TCSANOW:
		action = TIOCSETA;
		break;
	case TCSADRAIN:
		action = TIOCSETAW;
		break;
	case TCSAFLUSH:
		action = TIOCSETAF;
		break;
	default:
		errno = EINVAL;
		return (-1);
	}
	return (uioctl(fd, action, t));
}
