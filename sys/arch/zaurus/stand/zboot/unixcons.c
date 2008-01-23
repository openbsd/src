/*	$OpenBSD: unixcons.c,v 1.2 2008/01/23 16:37:57 jsing Exp $	*/

/*
 * Copyright (c) 1997-1999 Michael Shalayeff
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
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <dev/cons.h>
#include "libsa.h"
#include "unixdev.h"

struct termios tioc;
int	       tioc_valid = 0;

#define NCOM 3
const char *compath[NCOM] = {
	"/dev/ttyS0",		/* com0 */
	"/dev/ttyS2",		/* com1 */
	"/dev/ttyS1"		/* com2 */
};

int com_fd = -1;		/* open serial port */
int com_speed = 9600;		/* default speed is 9600 baud */

/* Local prototypes */
void	common_putc(dev_t, int);
int	common_getc(dev_t);


void
common_putc(dev_t dev, int c)
{
	/* Always send to stdout. */
	(void)uwrite(1, &c, 1);

	/* Copy to serial if open. */
	if (com_fd != -1)
		(void)uwrite(com_fd, &c, 1);
}

int
common_getc(dev_t dev)
{
	struct timeval tv;
	fd_set fdset;
	int fd, nfds, n;
	char c;

	while (1) {
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		FD_ZERO(&fdset);

		/* Always read from stdin. */
		fd = 0;
		nfds = 1;
		FD_SET(fd, &fdset);

		/* Read from serial if open. */
		if (com_fd != -1) {
			nfds = com_fd+1;
			FD_SET(com_fd, &fdset);
		}

		n = uselect(nfds, &fdset, NULL, NULL, &tv);
		if ((dev & 0x80) != 0)
			return (n > 0);

		if (n > 0)
			break;
	}

	for (fd = 0; fd < nfds; fd++)
		if (FD_ISSET(fd, &fdset))
			break;

	return (uread(fd, &c, 1) < 1 ? -1 : c);
}


void
cn_probe(struct consdev *cn)
{
	cn->cn_pri = CN_MIDPRI;
	cn->cn_dev = makedev(0,0);
	printf("cn%d ", minor(cn->cn_dev));
}

void
cn_init(struct consdev *cn)
{
	struct termios t;

	if (!tioc_valid && tcgetattr(0, &t) == 0) {
		tioc = t;
		tioc_valid = 1;
		cfmakeraw(&t);
		(void)tcsetattr(0, TCSAFLUSH, &t);
	}
}

void
cn_putc(dev_t dev, int c)
{
	common_putc(dev, c);
}

int
cn_getc(dev_t dev)
{
	return (common_getc(dev));
}


void
com_probe(struct consdev *cn)
{
	int i;
	struct linux_stat sb;

	for (i = 0; i < NCOM; i++) {
		if (ustat(compath[i], &sb) != 0)
			continue;
		printf("com%d ", i);
	}

	cn->cn_pri = CN_LOWPRI;
	/* XXX from arm/conf.c */
	cn->cn_dev = makedev(12, 0);
}

void
com_init(struct consdev *cn)
{
	struct termios t;
	int unit = minor(cn->cn_dev);
	
	if (unit >= NCOM)
		return;

	if (com_fd != -1)
		uclose(com_fd);
	
	com_fd = uopen(compath[unit], O_RDWR);
	if (com_fd == -1)
		return;

	if (tcgetattr(com_fd, &t) == 0) {
		cfmakeraw(&t);
		cfsetspeed(&t, (speed_t)com_speed);
		(void)tcsetattr(com_fd, TCSAFLUSH, &t);
	}
}

void
com_putc(dev_t dev, int c)
{
	common_putc(dev, c);
}

int
com_getc(dev_t dev)
{
	return (common_getc(dev));
}


int
cnspeed(dev_t dev, int sp)
{
	if (major(dev) == 12)	/* comN */
		return (comspeed(dev, sp));

	/* cn0 or anything else */
	return (9600);
}

/* call with sp == 0 to query the current speed */
int
comspeed(dev_t dev, int sp)
{
	struct termios t;

	if (sp <= 0)
		return (com_speed);

	/* check if the new speed is a valid baud rate */
	if (cfsetspeed(&t, (speed_t)sp) != 0)
		sp = com_speed;

	if (cn_tab && cn_tab->cn_dev == dev && com_speed != sp) {
		printf("com%d: changing speed to %d baud in 5 seconds, "
		    "change your terminal to match!\n\a",
		    minor(dev), sp);
		sleep(5);
		if (com_fd != -1 && tcgetattr(com_fd, &t) == 0) {
			(void)cfsetspeed(&t, (speed_t)sp);
			(void)tcsetattr(com_fd, TCSAFLUSH, &t);
		}
		printf("\n");
	}

	if (com_speed != sp) {
		printf("com%d: %d baud\n", minor(dev), sp);
		com_speed = sp;
	}

	return (com_speed);
}
