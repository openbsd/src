/*	$OpenBSD: client.h,v 1.2 2009/08/27 11:42:50 jacekm Exp $	*/

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

struct smtp_client;

/* return codes for io routines */
#define	CLIENT_DONE		 0	/* finished ok */
#define	CLIENT_ERROR		-1	/* generic error */
#define	CLIENT_WANT_READ	-2	/* need more data */
#define	CLIENT_WANT_WRITE	-3	/* have to send sth */

/* client states */
#define CLIENT_INIT		0x1
#define CLIENT_EHLO		0x2
#define CLIENT_HELO		0x3
#define CLIENT_MAILFROM		0x4
#define CLIENT_RCPTTO		0x5
#define CLIENT_DATA		0x6
#define CLIENT_DATA_BODY	0x7

struct rcpt {
	TAILQ_ENTRY(rcpt)	 entry;
	char			*mbox;
};

struct smtp_client {
	int			 state;
	char			*ehlo;
	char			*sender;
	TAILQ_HEAD(,rcpt)	 recipients;
	struct buf_read		 r;
	struct msgbuf		 w;
	struct buf		*data;
	FILE			*verbose;
	char			 errbuf[1024];
};

struct smtp_client	*client_init(int, char *);
void			 client_verbose(struct smtp_client *, int);
int			 client_sender(struct smtp_client *, char *, ...);
int			 client_rcpt(struct smtp_client *, char *, ...);
int			 client_data_fd(struct smtp_client *, int);
int			 client_data_printf(struct smtp_client *, char *, ...);
int			 client_read(struct smtp_client *, char **);
int			 client_write(struct smtp_client *, char **);
void			 client_close(struct smtp_client *);
