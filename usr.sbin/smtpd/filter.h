/*	$OpenBSD: filter.h,v 1.7 2012/01/18 13:41:54 chl Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
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

#include <sys/socket.h>

#include <netdb.h>

#define	FILTER_API_VERSION	50

#if !defined(MAX_LINE_SIZE)
#define MAX_LINE_SIZE		 1024
#endif

#if !defined(MAX_LOCALPART_SIZE)
#define MAX_LOCALPART_SIZE	 128
#endif

#if !defined(MAX_DOMAINPART_SIZE)
#define MAX_DOMAINPART_SIZE	 (MAX_LINE_SIZE-MAX_LOCALPART_SIZE)
#endif

enum filter_status {
	STATUS_IGNORE,
	STATUS_REJECT,
	STATUS_ACCEPT,
	STATUS_WAITING
};

enum filter_type {
	FILTER_CONNECT		= 0x001,
	FILTER_HELO		= 0x002,
	FILTER_EHLO		= 0x004,
	FILTER_MAIL		= 0x008,
	FILTER_RCPT		= 0x010,
	FILTER_DATALINE		= 0x020,
	FILTER_QUIT		= 0x040,
	FILTER_CLOSE		= 0x080,
	FILTER_RSET		= 0x100,
};

struct filter_connect {
	char			hostname[MAXHOSTNAMELEN];
	struct sockaddr_storage	hostaddr;
};

struct filter_helo {
	char			helohost[MAXHOSTNAMELEN];
};
 
struct filter_mail {
	char			user[MAX_LOCALPART_SIZE];
	char			domain[MAX_DOMAINPART_SIZE];
};

struct filter_rcpt {
	char			user[MAX_LOCALPART_SIZE];
	char			domain[MAX_DOMAINPART_SIZE];
};

struct filter_dataline {
	char			line[MAX_LINE_SIZE];
};

union filter_union {
	struct filter_connect	connect;
	struct filter_helo	helo;
	struct filter_mail	mail;
	struct filter_rcpt	rcpt;
	struct filter_dataline	dataline;
};

struct filter_msg {
	u_int64_t		id;	 /* set by smtpd(8) */
	u_int64_t		cl_id;	 /* set by smtpd(8) */
	int8_t       		code;
	u_int8_t		version;
	enum filter_type	type;
	union filter_union	u;
};

/**/
void filter_init(void);
void filter_loop(void);

void filter_register_connect_callback(enum filter_status (*)(u_int64_t, struct filter_connect *, void *), void *);
void filter_register_helo_callback(enum filter_status (*)(u_int64_t, struct filter_helo *, void *), void *);
void filter_register_ehlo_callback(enum filter_status (*)(u_int64_t, struct filter_helo *, void *), void *);
void filter_register_mail_callback(enum filter_status (*)(u_int64_t, struct filter_mail *, void *), void *);
void filter_register_rcpt_callback(enum filter_status (*)(u_int64_t, struct filter_rcpt *, void *), void *);
void filter_register_dataline_callback(enum filter_status (*)(u_int64_t, struct filter_dataline *, void *), void *);
void filter_register_quit_callback(enum filter_status (*)(u_int64_t, void *), void *);
void filter_register_close_callback(enum filter_status (*)(u_int64_t, void *), void *);
void filter_register_rset_callback(enum filter_status (*)(u_int64_t, void *), void *);

