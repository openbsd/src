/*	$OpenBSD: client.h,v 1.13 2010/11/28 13:56:43 gilles Exp $	*/

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
#define CLIENT_DONE		-1	/* finished */
#define CLIENT_WANT_WRITE	-2	/* want read + write */
#define CLIENT_STOP_WRITE	-3	/* want read */
#define CLIENT_RCPT_FAIL	-4	/* recipient refused */

/* client commands */
#define CLIENT_BANNER		0x1
#define CLIENT_EHLO		0x2
#define CLIENT_HELO		0x3
#define CLIENT_STARTTLS		0x4
#define CLIENT_AUTH		0x5
#define CLIENT_MAILFROM		0x6
#define CLIENT_RCPTTO		0x7
#define CLIENT_DATA		0x8
#define CLIENT_DOT		0x9
#define CLIENT_QUIT		0xa

struct client_cmd {
	TAILQ_ENTRY(client_cmd)	 entry;
	char			*action;
	int			 type;
	void			*data;
};
TAILQ_HEAD(cmdqueue, client_cmd);

/* smtp extensions */
#define CLIENT_EXT_STARTTLS	0
#define CLIENT_EXT_AUTH		1
#define CLIENT_EXT_PIPELINING	2

struct client_ext {
	short			 have;
	short			 want;
	short			 must;
	short			 done;
	short			 fail;
	char			*name;
};

struct client_auth {
	char			*plain;
	char			*cert;
	size_t			 certsz;
	char			*key;
	size_t			 keysz;
};

/* session flags */
#define CLIENT_FLAG_FIRSTTIME	0x1
#define CLIENT_FLAG_HANDSHAKING	0x2
#define CLIENT_FLAG_RCPTOKAY	0x4
#define CLIENT_FLAG_DYING	0x8

struct smtp_client {
	size_t			 cmdi;		/* iterator */
	size_t			 cmdw;		/* window */
	struct cmdqueue		 cmdsendq;	/* cmds to send */
	struct cmdqueue		 cmdrecvq;	/* replies waited for */

	int			 flags;
	void			*rcptfail;
	char			*ehlo;
	char			 reply[1024];
	struct ibuf_read	 r;
	struct msgbuf		 w;
	void			*ssl;
	int			 sndlowat;
	struct timeval		 timeout;
	FILE			*verbose;

	struct ibuf		*content;	/* current chunk of content */
	struct ibuf		*head;		/* headers + part of body */
	FILE			*body;		/* rest of body */

	struct client_ext	 exts[3];
	struct client_auth	 auth;

	char			 status[1024];
};

struct smtp_client	*client_init(int, int, char *, int);
void			 client_ssl_smtps(struct smtp_client *);
void			 client_ssl_optional(struct smtp_client *);
void			 client_certificate(struct smtp_client *, char *,
			     size_t, char *, size_t);
void			 client_auth(struct smtp_client *, char *);
void			 client_sender(struct smtp_client *, char *, ...);
void			 client_rcpt(struct smtp_client *, void *, char *, ...);
void			 client_printf(struct smtp_client *, char *, ...);
int			 client_talk(struct smtp_client *, int);
void			 client_close(struct smtp_client *);


struct client_cmd *cmd_new(int, char *, ...);
void		 cmd_free(struct client_cmd *);
int		 client_read(struct smtp_client *);
void		 client_get_reply(struct smtp_client *, struct client_cmd *,
    int *);
int		 client_write(struct smtp_client *);
int		 client_use_extensions(struct smtp_client *);
void		 client_status(struct smtp_client *, char *, ...);
int		 client_getln(struct smtp_client *, int);
void		 client_putln(struct smtp_client *, char *, ...);
struct ibuf	*client_content_read(FILE *, size_t);
int		 client_poll(struct smtp_client *);
void		 client_quit(struct smtp_client *);

int		 client_socket_read(struct smtp_client *);
int		 client_socket_write(struct smtp_client *);

char		*buf_getln(struct ibuf_read *);
int		 buf_read(int, struct ibuf_read *);
