/*	$OpenBSD: client.h,v 1.5 2009/12/12 10:33:11 jacekm Exp $	*/

/*
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

struct smtp_client;

/* return codes for io routines */
#define CLIENT_DONE		 0	/* finished ok */
#define CLIENT_WANT_READ	-1	/* need more data */
#define CLIENT_WANT_WRITE	-2	/* have to send sth */
#define CLIENT_RCPT_FAIL	-3	/* recipient refused */

/* client states */
#define CLIENT_SSL_INIT		0x1
#define CLIENT_SSL_CONNECT	0x2
#define CLIENT_INIT		0x3
#define CLIENT_EHLO		0x4
#define CLIENT_HELO		0x5
#define CLIENT_STARTTLS		0x6
#define CLIENT_AUTH		0x7
#define CLIENT_MAILFROM		0x8
#define CLIENT_RCPTTO		0x9
#define CLIENT_DATA		0xa
#define CLIENT_DATA_BODY	0xb
#define CLIENT_QUIT		0xc

/* smtp extensions */
#define CLIENT_EXT_STARTTLS	0
#define CLIENT_EXT_AUTH		1
#define CLIENT_EXT_MAX		2

struct rcpt {
	TAILQ_ENTRY(rcpt)	 entry;
	char			*mbox;
	void			*p;
};

struct client_auth {
	char			*plain;
	char			*cert;
	size_t			 certsz;
	char			*key;
	size_t			 keysz;
};

struct client_ext {
	short			 have;
	short			 want;
	short			 must;
	short			 done;
	short			 fail;
	char			*name;
	int			 state;
};

struct smtp_client {
	int			 state;
	char			*ehlo;
	char			*sender;
	TAILQ_HEAD(rlist,rcpt)	 recipients;
	struct rcpt		*rcpt;
	struct rcpt		*rcptfail;
	size_t			 rcptokay;
	struct buf_read		 r;
	struct msgbuf		 w;
	struct buf		*data;
	struct client_ext	 exts[CLIENT_EXT_MAX];
	int			(*handler)(struct smtp_client *);
	void			*ssl_state;
	struct client_auth	 auth;
	struct timeval		 timeout;
	char			 reply[1024];
	char			 status[1024];
	FILE			*verbose;
};

struct smtp_client	*client_init(int, char *, int);
void			 client_ssl_smtps(struct smtp_client *);
void			 client_ssl_optional(struct smtp_client *);
void			 client_certificate(struct smtp_client *, char *,
			     size_t, char *, size_t);
void			 client_auth(struct smtp_client *, char *);
void			 client_sender(struct smtp_client *, char *, ...);
void			 client_rcpt(struct smtp_client *, void *, char *, ...);
void			 client_data_fd(struct smtp_client *, int);
void			 client_data_printf(struct smtp_client *, char *, ...);
int			 client_talk(struct smtp_client *);
void			 client_close(struct smtp_client *);
