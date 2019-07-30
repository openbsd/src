/* $OpenBSD: ttymodes.c,v 1.33 2018/02/16 04:43:11 dtucker Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

/*
 * SSH2 tty modes support by Kevin Steves.
 * Copyright (c) 2001 Kevin Steves.  All rights reserved.
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

/*
 * Encoding and decoding of terminal modes in a portable way.
 * Much of the format is defined in ttymodes.h; it is included multiple times
 * into this file with the appropriate macro definitions to generate the
 * suitable code.
 */

#include <sys/types.h>

#include <errno.h>
#include <string.h>
#include <termios.h>
#include <stdarg.h>

#include "packet.h"
#include "log.h"
#include "compat.h"
#include "buffer.h"
#include "compat.h"

#define TTY_OP_END		0
/*
 * uint32 (u_int) follows speed.
 */
#define TTY_OP_ISPEED	128
#define TTY_OP_OSPEED	129

/*
 * Converts POSIX speed_t to a baud rate.  The values of the
 * constants for speed_t are not themselves portable.
 */
static int
speed_to_baud(speed_t speed)
{
	switch (speed) {
	case B0:
		return 0;
	case B50:
		return 50;
	case B75:
		return 75;
	case B110:
		return 110;
	case B134:
		return 134;
	case B150:
		return 150;
	case B200:
		return 200;
	case B300:
		return 300;
	case B600:
		return 600;
	case B1200:
		return 1200;
	case B1800:
		return 1800;
	case B2400:
		return 2400;
	case B4800:
		return 4800;
	case B9600:
		return 9600;

#ifdef B19200
	case B19200:
		return 19200;
#else /* B19200 */
#ifdef EXTA
	case EXTA:
		return 19200;
#endif /* EXTA */
#endif /* B19200 */

#ifdef B38400
	case B38400:
		return 38400;
#else /* B38400 */
#ifdef EXTB
	case EXTB:
		return 38400;
#endif /* EXTB */
#endif /* B38400 */

#ifdef B7200
	case B7200:
		return 7200;
#endif /* B7200 */
#ifdef B14400
	case B14400:
		return 14400;
#endif /* B14400 */
#ifdef B28800
	case B28800:
		return 28800;
#endif /* B28800 */
#ifdef B57600
	case B57600:
		return 57600;
#endif /* B57600 */
#ifdef B76800
	case B76800:
		return 76800;
#endif /* B76800 */
#ifdef B115200
	case B115200:
		return 115200;
#endif /* B115200 */
#ifdef B230400
	case B230400:
		return 230400;
#endif /* B230400 */
	default:
		return 9600;
	}
}

/*
 * Converts a numeric baud rate to a POSIX speed_t.
 */
static speed_t
baud_to_speed(int baud)
{
	switch (baud) {
	case 0:
		return B0;
	case 50:
		return B50;
	case 75:
		return B75;
	case 110:
		return B110;
	case 134:
		return B134;
	case 150:
		return B150;
	case 200:
		return B200;
	case 300:
		return B300;
	case 600:
		return B600;
	case 1200:
		return B1200;
	case 1800:
		return B1800;
	case 2400:
		return B2400;
	case 4800:
		return B4800;
	case 9600:
		return B9600;

#ifdef B19200
	case 19200:
		return B19200;
#else /* B19200 */
#ifdef EXTA
	case 19200:
		return EXTA;
#endif /* EXTA */
#endif /* B19200 */

#ifdef B38400
	case 38400:
		return B38400;
#else /* B38400 */
#ifdef EXTB
	case 38400:
		return EXTB;
#endif /* EXTB */
#endif /* B38400 */

#ifdef B7200
	case 7200:
		return B7200;
#endif /* B7200 */
#ifdef B14400
	case 14400:
		return B14400;
#endif /* B14400 */
#ifdef B28800
	case 28800:
		return B28800;
#endif /* B28800 */
#ifdef B57600
	case 57600:
		return B57600;
#endif /* B57600 */
#ifdef B76800
	case 76800:
		return B76800;
#endif /* B76800 */
#ifdef B115200
	case 115200:
		return B115200;
#endif /* B115200 */
#ifdef B230400
	case 230400:
		return B230400;
#endif /* B230400 */
	default:
		return B9600;
	}
}

/*
 * Encodes terminal modes for the terminal referenced by fd
 * or tiop in a portable manner, and appends the modes to a packet
 * being constructed.
 */
void
tty_make_modes(int fd, struct termios *tiop)
{
	struct termios tio;
	int baud;
	Buffer buf;

	buffer_init(&buf);

	if (tiop == NULL) {
		if (fd == -1) {
			debug("tty_make_modes: no fd or tio");
			goto end;
		}
		if (tcgetattr(fd, &tio) == -1) {
			logit("tcgetattr: %.100s", strerror(errno));
			goto end;
		}
	} else
		tio = *tiop;

	/* Store input and output baud rates. */
	baud = speed_to_baud(cfgetospeed(&tio));
	buffer_put_char(&buf, TTY_OP_OSPEED);
	buffer_put_int(&buf, baud);
	baud = speed_to_baud(cfgetispeed(&tio));
	buffer_put_char(&buf, TTY_OP_ISPEED);
	buffer_put_int(&buf, baud);

	/* Store values of mode flags. */
#define TTYCHAR(NAME, OP) \
	buffer_put_char(&buf, OP); \
	buffer_put_int(&buf, tio.c_cc[NAME]);

#define SSH_TTYMODE_IUTF8 42  /* for SSH_BUG_UTF8TTYMODE */

#define TTYMODE(NAME, FIELD, OP) \
	if (OP == SSH_TTYMODE_IUTF8 && (datafellows & SSH_BUG_UTF8TTYMODE)) { \
		debug3("%s: SSH_BUG_UTF8TTYMODE", __func__); \
	} else { \
		buffer_put_char(&buf, OP); \
		buffer_put_int(&buf, ((tio.FIELD & NAME) != 0)); \
	}

#include "ttymodes.h"

#undef TTYCHAR
#undef TTYMODE

end:
	/* Mark end of mode data. */
	buffer_put_char(&buf, TTY_OP_END);
	packet_put_string(buffer_ptr(&buf), buffer_len(&buf));
	buffer_free(&buf);
}

/*
 * Decodes terminal modes for the terminal referenced by fd in a portable
 * manner from a packet being read.
 */
void
tty_parse_modes(int fd, int *n_bytes_ptr)
{
	struct termios tio;
	int opcode, baud;
	int n_bytes = 0;
	int failure = 0;

	*n_bytes_ptr = packet_get_int();
	if (*n_bytes_ptr == 0)
		return;

	/*
	 * Get old attributes for the terminal.  We will modify these
	 * flags. I am hoping that if there are any machine-specific
	 * modes, they will initially have reasonable values.
	 */
	if (tcgetattr(fd, &tio) == -1) {
		logit("tcgetattr: %.100s", strerror(errno));
		failure = -1;
	}

	for (;;) {
		n_bytes += 1;
		opcode = packet_get_char();
		switch (opcode) {
		case TTY_OP_END:
			goto set;

		case TTY_OP_ISPEED:
			n_bytes += 4;
			baud = packet_get_int();
			if (failure != -1 &&
			    cfsetispeed(&tio, baud_to_speed(baud)) == -1)
				error("cfsetispeed failed for %d", baud);
			break;

		case TTY_OP_OSPEED:
			n_bytes += 4;
			baud = packet_get_int();
			if (failure != -1 &&
			    cfsetospeed(&tio, baud_to_speed(baud)) == -1)
				error("cfsetospeed failed for %d", baud);
			break;

#define TTYCHAR(NAME, OP) \
	case OP: \
	  n_bytes += 4; \
	  tio.c_cc[NAME] = packet_get_int(); \
	  break;
#define TTYMODE(NAME, FIELD, OP) \
	case OP: \
	  n_bytes += 4; \
	  if (packet_get_int()) \
	    tio.FIELD |= NAME; \
	  else \
	    tio.FIELD &= ~NAME;	\
	  break;

#include "ttymodes.h"

#undef TTYCHAR
#undef TTYMODE

		default:
			debug("Ignoring unsupported tty mode opcode %d (0x%x)",
			    opcode, opcode);
			/*
			 * SSH2:
			 * Opcodes 1 to 159 are defined to have a uint32
			 * argument.
			 * Opcodes 160 to 255 are undefined and cause parsing
			 * to stop.
			 */
			if (opcode > 0 && opcode < 160) {
				n_bytes += 4;
				(void) packet_get_int();
				break;
			} else {
				logit("parse_tty_modes: unknown opcode %d",
				    opcode);
				goto set;
			}
		}
	}

set:
	if (*n_bytes_ptr != n_bytes) {
		*n_bytes_ptr = n_bytes;
		logit("parse_tty_modes: n_bytes_ptr != n_bytes: %d %d",
		    *n_bytes_ptr, n_bytes);
		return;		/* Don't process bytes passed */
	}
	if (failure == -1)
		return;		/* Packet parsed ok but tcgetattr() failed */

	/* Set the new modes for the terminal. */
	if (tcsetattr(fd, TCSANOW, &tio) == -1)
		logit("Setting tty modes failed: %.100s", strerror(errno));
}
