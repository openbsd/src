/* $OpenBSD: xmodem.c,v 1.8 2015/10/05 23:15:31 nicm Exp $ */

/*
 * Copyright (c) 2012 Nicholas Marriott <nicm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "cu.h"

#define XMODEM_BLOCK 128
#define XMODEM_RETRIES 10

#define XMODEM_SOH '\001'
#define XMODEM_EOT '\004'
#define XMODEM_ACK '\006'
#define XMODEM_NAK '\025'
#define XMODEM_SUB '\032'
#define XMODEM_C   '\103'

volatile sig_atomic_t xmodem_stop;

void		xmodem_signal(int sig);
uint16_t	xmodem_crc16(const u_char *buf, size_t len);
int		xmodem_read(char *c);
int		xmodem_write(const u_char *buf, size_t len);

void
xmodem_signal(int sig)
{
	xmodem_stop = 1;
}

uint16_t
xmodem_crc16(const u_char *buf, size_t len)
{
	uint16_t	crc;
	u_int		i, j;

	crc = 0;
	for (i = 0; i < len; i++) {
		crc = crc ^ *buf++ << 8;
		for (j = 0; j < 8; j++)
			if (crc & 0x8000)
				crc = crc << 1 ^ 0x1021;
			else
				crc = crc << 1;
	}
	return (crc);
}

int
xmodem_read(char *c)
{
	for (;;) {
		switch (read(line_fd, c, 1)) {
		case -1:
			if (errno == EINTR && !xmodem_stop)
				continue;
			return (-1);
		case 0:
			errno = EPIPE;
			return (-1);
		case 1:
			return (0);
		}
	}
}

int
xmodem_write(const u_char *buf, size_t len)
{
	ssize_t	n;

	while (len > 0) {
		n = write(line_fd, buf, len);
		if (n == -1) {
			if (errno == EINTR && !xmodem_stop)
				continue;
			return (-1);
		}
		buf += n;
		len -= n;
	}
	return (0);
}

void
xmodem_send(const char *file)
{
	FILE			*f;
	u_char			 buf[3 + XMODEM_BLOCK + 2], c;
	size_t			 len, pktlen;
	uint8_t			 num;
	uint16_t		 crc;
	int			 crc_mode;
	u_int			 i, total;
	struct termios		 tio;
	struct sigaction	 act, oact;

	f = fopen(file, "r");
	if (f == NULL) {
		cu_warn("%s", file);
		return;
	}

	memset(&act, 0, sizeof(act));
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = xmodem_signal;
	if (sigaction(SIGINT, &act, &oact) != 0)
		cu_err(1, "sigaction");
	xmodem_stop = 0;

	if (isatty(STDIN_FILENO)) {
		memcpy(&tio, &saved_tio, sizeof(tio));
		tio.c_lflag &= ~ECHO;
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio) != 0)
			cu_err(1, "tcsetattr");
	}
	set_blocking(line_fd, 1);
	tcflush(line_fd, TCIFLUSH);

	if (xmodem_read(&c) != 0)
		goto fail;
	if (c == XMODEM_C)
		crc_mode = 1;
	else if (c == XMODEM_NAK)
		crc_mode = 0;
	else {
		cu_warnx("%s: unexpected response \\%03hho", file, c);
		goto fail;
	}

	num = 1;
	total = 1;
	pktlen = 3 + XMODEM_BLOCK + (crc_mode ? 2 : 1);
	for (;;) {
		len = fread(buf + 3, 1, XMODEM_BLOCK, f);
		if (len == 0)
			break;
		memset(buf + 3 + len, XMODEM_SUB, XMODEM_BLOCK - len);

		buf[0] = XMODEM_SOH;
		buf[1] = num;
		buf[2] = 255 - num;

		if (crc_mode) {
			crc = xmodem_crc16(buf + 3, XMODEM_BLOCK);
			buf[3 + XMODEM_BLOCK] = crc >> 8;
			buf[3 + XMODEM_BLOCK + 1] = crc & 0xFF;
		} else {
			buf[3 + XMODEM_BLOCK] = 0;
			for (i = 0; i < XMODEM_BLOCK; i++)
				buf[3 + XMODEM_BLOCK] += buf[3 + i];
		}

		for (i = 0; i < XMODEM_RETRIES; i++) {
			if (xmodem_stop) {
				errno = EINTR;
				goto fail;
			}
			cu_warnx("%s: sending block %u (attempt %u)", file,
			    total, 1 + i);
			if (xmodem_write(buf, pktlen) != 0)
				goto fail;

			if (xmodem_read(&c) != 0)
				goto fail;
			if (c == XMODEM_ACK)
				break;
			if (c != XMODEM_NAK) {
				cu_warnx("%s: unexpected response \\%03hho",
				    file, c);
			}
		}
		if (i == XMODEM_RETRIES) {
			cu_warnx("%s: too many retries", file);
			goto out;
		}

		if (len < XMODEM_BLOCK)
			break;
		num++;
		total++;
	}

	buf[0] = XMODEM_EOT;
	if (xmodem_write(buf, 1) != 0)
		goto fail;
	cu_warnx("%s: completed %u blocks", file, num);

	goto out;

fail:
	cu_warn("%s", file);

out:
	set_blocking(line_fd, 0);
	set_termios();

	sigaction(SIGINT, &oact, NULL);

	fclose(f);
}
