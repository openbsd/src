/*	$OpenBSD: session.h,v 1.3 2004/01/01 23:46:47 henning Exp $ */

/*
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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

#include <sys/types.h>

#define	MAX_BACKLOG			5
#define	INTERVAL_CONNECTRETRY		120
#define	INTERVAL_HOLD_INITIAL		240
#define	INTERVAL_HOLD			90
#define INTERVAL_START			90
#define MSGSIZE_HEADER			19
#define MSGSIZE_HEADER_MARKER		16
#define	MSGSIZE_NOTIFICATION_MIN	21	/* 19 hdr + 1 code + 1 sub */
#define	MSGSIZE_OPEN_MIN		29
#define	MSGSIZE_UPDATE_MIN		23
#define	MSGSIZE_KEEPALIVE		MSGSIZE_HEADER

enum msg_type {
	OPEN = 1,
	UPDATE,
	NOTIFICATION,
	KEEPALIVE
};

enum err_codes {
	ERR_HEADER = 1,
	ERR_OPEN,
	ERR_UPDATE,
	ERR_HOLDTIMEREXPIRED,
	ERR_FSM,
	ERR_CEASE
};

enum suberr_header {
	ERR_HDR_SYNC = 1,
	ERR_HDR_LEN,
	ERR_HDR_TYPE
};

enum suberr_open {
	ERR_OPEN_VERSION = 1,
	ERR_OPEN_AS,
	ERR_OPEN_BGPID,
	ERR_OPEN_OPT,
	ERR_OPEN_AUTH,
	ERR_OPEN_HOLDTIME
};

struct msg_header {
	u_char			 marker[16];
	u_int16_t		 len;
	u_int8_t		 type;
};

struct msg_open {
	struct msg_header	 header;
	u_int8_t		 version;
	u_int16_t		 myas;
	u_int16_t		 holdtime;
	u_int32_t		 bgpid;
	u_int8_t		 optparamlen;
};

struct ctl_conn {
	TAILQ_ENTRY(ctl_conn)	entries;
	struct imsgbuf		ibuf;
};

TAILQ_HEAD(ctl_conns, ctl_conn)	ctl_conns;

struct bgpd_config	*conf;

/* control.c */
int	control_listen(void);
void	control_shutdown(void);
int	control_dispatch_msg(struct pollfd *, int);
void	control_accept(int);
void	control_close(int);
