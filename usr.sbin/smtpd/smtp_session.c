/*	$OpenBSD: smtp_session.c,v 1.226 2015/01/16 06:40:21 deraadt Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008-2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <limits.h>
#include <inttypes.h>
#include <openssl/ssl.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "smtpd.h"
#include "log.h"
#include "ssl.h"

#define SMTP_LIMIT_MAIL		100
#define SMTP_LIMIT_RCPT		1000

#define SMTP_KICK_CMD		5
#define SMTP_KICK_RCPTFAIL	50

#define	APPEND_DOMAIN_BUFFER_SIZE	4096

enum smtp_phase {
	PHASE_INIT = 0,
	PHASE_SETUP,
	PHASE_TRANSACTION
};

enum smtp_state {
	STATE_NEW = 0,
	STATE_CONNECTED,
	STATE_TLS,
	STATE_HELO,
	STATE_AUTH_INIT,
	STATE_AUTH_USERNAME,
	STATE_AUTH_PASSWORD,
	STATE_AUTH_FINALIZE,
	STATE_BODY,
	STATE_QUIT,
};

enum session_flags {
	SF_EHLO			= 0x0001,
	SF_8BITMIME		= 0x0002,
	SF_SECURE		= 0x0004,
	SF_AUTHENTICATED	= 0x0008,
	SF_BOUNCE		= 0x0010,
	SF_KICK			= 0x0020,
	SF_VERIFIED		= 0x0040,
	SF_MFACONNSENT		= 0x0080,
	SF_BADINPUT		= 0x0100,
};

enum message_flags {
	MF_QUEUE_ENVELOPE_FAIL	= 0x00001,
	MF_ERROR_SIZE		= 0x01000,
	MF_ERROR_IO		= 0x02000,
	MF_ERROR_LOOP		= 0x04000,
	MF_ERROR_MALFORMED     	= 0x08000,
	MF_ERROR_RESOURCES     	= 0x10000,
};
#define MF_ERROR	(MF_ERROR_SIZE | MF_ERROR_IO | MF_ERROR_LOOP | MF_ERROR_MALFORMED | MF_ERROR_RESOURCES)

enum smtp_command {
	CMD_HELO = 0,
	CMD_EHLO,
	CMD_STARTTLS,
	CMD_AUTH,
	CMD_MAIL_FROM,
	CMD_RCPT_TO,
	CMD_DATA,
	CMD_RSET,
	CMD_QUIT,
	CMD_HELP,
	CMD_WIZ,
	CMD_NOOP,
};

struct smtp_rcpt {
	TAILQ_ENTRY(smtp_rcpt)	 entry;
 	struct mailaddr		 maddr;
	size_t			 destcount;
};

struct smtp_session {
	uint64_t		 id;
	struct iobuf		 iobuf;
	struct io		 io;
	struct listener		*listener;
	void			*ssl_ctx;
	struct sockaddr_storage	 ss;
	char			 hostname[SMTPD_MAXHOSTNAMELEN];
	char			 smtpname[SMTPD_MAXHOSTNAMELEN];
	char			 sni[SMTPD_MAXHOSTNAMELEN];

	int			 flags;
	int			 phase;
	enum smtp_state		 state;

	char			 helo[SMTPD_MAXLINESIZE];
	char			 cmd[SMTPD_MAXLINESIZE];
	char			 username[SMTPD_MAXLOGNAME];

	struct envelope		 evp;

	size_t			 kickcount;
	size_t			 mailcount;

	int			 msgflags;
	int			 msgcode;
	size_t			 rcptcount;
	size_t			 destcount;
	size_t			 rcptfail;
	TAILQ_HEAD(, smtp_rcpt)	 rcpts;

	size_t			 datalen;
	FILE			*ofile;
	int			 hdrdone;
	int			 rcvcount;

	struct event		 pause;

	struct rfc2822_parser	 rfc2822_parser;
};

#define ADVERTISE_TLS(s) \
	((s)->listener->flags & F_STARTTLS && !((s)->flags & SF_SECURE))

#define ADVERTISE_AUTH(s) \
	((s)->listener->flags & F_AUTH && (s)->flags & SF_SECURE && \
	 !((s)->flags & SF_AUTHENTICATED))

#define ADVERTISE_EXT_DSN(s) \
	((s)->listener->flags & F_EXT_DSN)

static int smtp_mailaddr(struct mailaddr *, char *, int, char **, const char *);
static void smtp_session_init(void);
static int smtp_lookup_servername(struct smtp_session *);
static void smtp_connected(struct smtp_session *);
static void smtp_send_banner(struct smtp_session *);
static void smtp_mfa_response(struct smtp_session *, int, int, uint32_t,
    const char *);
static void smtp_io(struct io *, int);
static void smtp_enter_state(struct smtp_session *, int);
static void smtp_reply(struct smtp_session *, char *, ...);
static void smtp_command(struct smtp_session *, char *);
static int smtp_parse_mail_args(struct smtp_session *, char *);
static int smtp_parse_rcpt_args(struct smtp_session *, char *);
static void smtp_rfc4954_auth_plain(struct smtp_session *, char *);
static void smtp_rfc4954_auth_login(struct smtp_session *, char *);
static void smtp_message_write(struct smtp_session *, const char *);
static void smtp_message_end(struct smtp_session *);
static void smtp_message_reset(struct smtp_session *, int);
static void smtp_free(struct smtp_session *, const char *);
static const char *smtp_strstate(int);
static int smtp_verify_certificate(struct smtp_session *);
static uint8_t dsn_notify_str_to_uint8(const char *);
static void smtp_auth_failure_pause(struct smtp_session *);
static void smtp_auth_failure_resume(int, short, void *);
static int smtp_sni_callback(SSL *, int *, void *);

static void smtp_filter_connect(struct smtp_session *, struct sockaddr *);
static void smtp_filter_rset(struct smtp_session *);
static void smtp_filter_disconnect(struct smtp_session *);
static void smtp_filter_commit(struct smtp_session *);
static void smtp_filter_rollback(struct smtp_session *);
static void smtp_filter_eom(struct smtp_session *);
static void smtp_filter_helo(struct smtp_session *);
static void smtp_filter_mail(struct smtp_session *s);
static void smtp_filter_rcpt(struct smtp_session *s);
static void smtp_filter_data(struct smtp_session *s);

static struct { int code; const char *cmd; } commands[] = {
	{ CMD_HELO,		"HELO" },
	{ CMD_EHLO,		"EHLO" },
	{ CMD_STARTTLS,		"STARTTLS" },
	{ CMD_AUTH,		"AUTH" },
	{ CMD_MAIL_FROM,	"MAIL FROM" },
	{ CMD_RCPT_TO,		"RCPT TO" },
	{ CMD_DATA,		"DATA" },
	{ CMD_RSET,		"RSET" },
	{ CMD_QUIT,		"QUIT" },
	{ CMD_HELP,		"HELP" },
	{ CMD_WIZ,		"WIZ" },
	{ CMD_NOOP,		"NOOP" },
	{ -1, NULL },
};

static struct tree wait_lka_ptr;
static struct tree wait_lka_helo;
static struct tree wait_lka_rcpt;
static struct tree wait_mfa_data;
static struct tree wait_parent_auth;
static struct tree wait_queue_msg;
static struct tree wait_queue_fd;
static struct tree wait_queue_commit;
static struct tree wait_ssl_init;
static struct tree wait_ssl_verify;

static void
header_default_callback(const struct rfc2822_header *hdr, void *arg)
{
	struct smtp_session    *s = arg;
	struct rfc2822_line    *l;
	size_t			len;

	len = strlen(hdr->name) + 1;
	if (fprintf(s->ofile, "%s:", hdr->name) != (int)len) {
		s->msgflags |= MF_ERROR_IO;
		return;
	}
	s->datalen += len;

	TAILQ_FOREACH(l, &hdr->lines, next) {
		len = strlen(l->buffer) + 1;
		if (fprintf(s->ofile, "%s\n", l->buffer) != (int)len) {
			s->msgflags |= MF_ERROR_IO;
			return;
		}
		s->datalen += len;
	}
}

static void
dataline_callback(const char *line, void *arg)
{
	struct smtp_session	*s = arg;
	size_t			len;

	len = strlen(line) + 1;

	if (s->datalen + len > env->sc_maxsize) {
		s->msgflags |= MF_ERROR_SIZE;
		return;
	}

	if (fprintf(s->ofile, "%s\n", line) != (int)len) {
		s->msgflags |= MF_ERROR_IO;
		return;
	}

	s->datalen += len;
}

static void
header_bcc_callback(const struct rfc2822_header *hdr, void *arg)
{
}

static void
header_append_domain_buffer(char *buffer, char *domain, size_t len)
{
	size_t	i;
	int	escape, quote, comment, bracket;
	int	has_domain, has_bracket, has_group;
	int	pos_bracket, pos_component, pos_insert;
	char	copy[APPEND_DOMAIN_BUFFER_SIZE];

	i = 0;
	escape = quote = comment = bracket = 0;
	has_domain = has_bracket = has_group = 0;
	pos_bracket = pos_insert = pos_component = 0;
	for (i = 0; buffer[i]; ++i) {
		if (buffer[i] == '(' && !escape && !quote)
			comment++;
		if (buffer[i] == '"' && !escape && !comment)
			quote = !quote;
		if (buffer[i] == ')' && !escape && !quote && comment)
			comment--;
		if (buffer[i] == '\\' && !escape && !comment && !quote)
			escape = 1;
		else
			escape = 0;
		if (buffer[i] == '<' && !escape && !comment && !quote && !bracket) {
			bracket++;
			has_bracket = 1;
		}
		if (buffer[i] == '>' && !escape && !comment && !quote && bracket) {
			bracket--;
			pos_bracket = i;
		}
		if (buffer[i] == '@' && !escape && !comment && !quote)
			has_domain = 1;
		if (buffer[i] == ':' && !escape && !comment && !quote)
			has_group = 1;

		/* update insert point if not in comment and not on a whitespace */
		if (!comment && buffer[i] != ')' && !isspace((unsigned char)buffer[i]))
			pos_component = i;
	}

	/* parse error, do not attempt to modify */
	if (escape || quote || comment || bracket)
		return;

	/* domain already present, no need to modify */
	if (has_domain)
		return;

	/* address is group, skip */
	if (has_group)
		return;

	/* there's an address between brackets, just append domain */
	if (has_bracket) {
		pos_bracket--;
		while (isspace((unsigned char)buffer[pos_bracket]))
			pos_bracket--;
		if (buffer[pos_bracket] == '<')
			return;
		pos_insert = pos_bracket + 1;
	}
	else {
		/* otherwise append address to last component */
		pos_insert = pos_component + 1;

		/* empty address */
                if (buffer[pos_component] == '\0' ||
		    isspace((unsigned char)buffer[pos_component]))
                        return;
	}

	if (snprintf(copy, sizeof copy, "%.*s@%s%s",
		(int)pos_insert, buffer,
		domain,
		buffer+pos_insert) >= (int)sizeof copy)
		return;

	memcpy(buffer, copy, len);
}

static void
header_masquerade_callback(const struct rfc2822_header *hdr, void *arg)
{
	struct smtp_session    *s = arg;
	struct rfc2822_line    *l;
	size_t			i, j;
	int			escape, quote, comment, skip;
	size_t			len;
	char			buffer[APPEND_DOMAIN_BUFFER_SIZE];

	len = strlen(hdr->name) + 1;
	if (fprintf(s->ofile, "%s:", hdr->name) != (int)len)
		goto ioerror;
	s->datalen += len;

	i = j = 0;
	escape = quote = comment = skip = 0;
	memset(buffer, 0, sizeof buffer);

	TAILQ_FOREACH(l, &hdr->lines, next) {
		for (i = 0; i < strlen(l->buffer); ++i) {
			if (l->buffer[i] == '(' && !escape && !quote)
				comment++;
			if (l->buffer[i] == '"' && !escape && !comment)
				quote = !quote;
			if (l->buffer[i] == ')' && !escape && !quote && comment)
				comment--;
			if (l->buffer[i] == '\\' && !escape && !comment && !quote)
				escape = 1;
			else
				escape = 0;

			/* found a separator, buffer contains a full address */
			if (l->buffer[i] == ',' && !escape && !quote && !comment) {
				if (!skip && j + strlen(s->listener->hostname) + 1 < sizeof buffer)
					header_append_domain_buffer(buffer, s->listener->hostname, sizeof buffer);
				len = strlen(buffer) + 1;
				if (fprintf(s->ofile, "%s,", buffer) != (int)len)
					goto ioerror;
				s->datalen += len;
				j = 0;
				skip = 0;
				memset(buffer, 0, sizeof buffer);
			}
			else {
				if (skip) {
					if (fprintf(s->ofile, "%c", l->buffer[i]) != (int)1)
						goto ioerror;
					s->datalen += 1;
				}
				else {
					buffer[j++] = l->buffer[i];
					if (j == sizeof (buffer) - 1) {
						len = strlen(buffer);
						if (fprintf(s->ofile, "%s", buffer) != (int)len)
							goto ioerror;
						s->datalen += len;
						skip = 1;
						j = 0;
						memset(buffer, 0, sizeof buffer);
					}
				}
			}
		}
		if (skip) {
			if (fprintf(s->ofile, "\n") != (int)1)
				goto ioerror;
			s->datalen += 1;
		}
		else {
			buffer[j++] = '\n';
			if (j == sizeof (buffer) - 1) {
				len = strlen(buffer);
				if (fprintf(s->ofile, "%s", buffer) != (int)len)
					goto ioerror;
				s->datalen += len;
				skip = 1;
				j = 0;
				memset(buffer, 0, sizeof buffer);
			}
		}
	}

	/* end of header, if buffer is not empty we'll process it */
	if (buffer[0]) {
		if (j + strlen(s->listener->hostname) + 1 < sizeof buffer)
			header_append_domain_buffer(buffer, s->listener->hostname, sizeof buffer);
		len = strlen(buffer);
		if (fprintf(s->ofile, "%s", buffer) != (int)len)
			goto ioerror;
		s->datalen += len;
	}
	return;

ioerror:
	s->msgflags |= MF_ERROR_IO;
	return;
}

static void
smtp_session_init(void)
{
	static int	init = 0;

	if (!init) {
		tree_init(&wait_lka_ptr);
		tree_init(&wait_lka_helo);
		tree_init(&wait_lka_rcpt);
		tree_init(&wait_mfa_data);
		tree_init(&wait_parent_auth);
		tree_init(&wait_queue_msg);
		tree_init(&wait_queue_fd);
		tree_init(&wait_queue_commit);
		tree_init(&wait_ssl_init);
		tree_init(&wait_ssl_verify);
		init = 1;
	}
}

int
smtp_session(struct listener *listener, int sock,
    const struct sockaddr_storage *ss, const char *hostname)
{
	struct smtp_session	*s;

	log_debug("debug: smtp: new client on listener: %p", listener);

	smtp_session_init();

	if ((s = calloc(1, sizeof(*s))) == NULL)
		return (-1);
	if (iobuf_init(&s->iobuf, SMTPD_MAXLINESIZE, SMTPD_MAXLINESIZE) == -1) {
		free(s);
		return (-1);
	}
	TAILQ_INIT(&s->rcpts);

	s->id = generate_uid();
	s->listener = listener;
	memmove(&s->ss, ss, sizeof(*ss));
	io_init(&s->io, sock, s, smtp_io, &s->iobuf);
	io_set_timeout(&s->io, SMTPD_SESSION_TIMEOUT * 1000);
	io_set_write(&s->io);

	s->state = STATE_NEW;
	s->phase = PHASE_INIT;

	(void)strlcpy(s->smtpname, listener->hostname, sizeof(s->smtpname));

	/* Setup parser and callbacks before smtp_connected() can be called */
	rfc2822_parser_init(&s->rfc2822_parser);
	rfc2822_header_default_callback(&s->rfc2822_parser,
	    header_default_callback, s);
	rfc2822_header_callback(&s->rfc2822_parser, "bcc",
	    header_bcc_callback, s);
	rfc2822_header_callback(&s->rfc2822_parser, "from",
	    header_masquerade_callback, s);
	rfc2822_header_callback(&s->rfc2822_parser, "to",
	    header_masquerade_callback, s);
	rfc2822_header_callback(&s->rfc2822_parser, "cc",
	    header_masquerade_callback, s);
	rfc2822_body_callback(&s->rfc2822_parser,
	    dataline_callback, s);

	/* For local enqueueing, the hostname is already set */
	if (hostname) {
		s->flags |= SF_AUTHENTICATED;
		/* A bit of a hack */
		if (!strcmp(hostname, "localhost"))
			s->flags |= SF_BOUNCE;
		(void)strlcpy(s->hostname, hostname, sizeof(s->hostname));
		if (smtp_lookup_servername(s))
			smtp_connected(s);
	} else {
		m_create(p_lka,  IMSG_SMTP_DNS_PTR, 0, 0, -1);
		m_add_id(p_lka, s->id);
		m_add_sockaddr(p_lka, (struct sockaddr *)&s->ss);
		m_close(p_lka);
		tree_xset(&wait_lka_ptr, s->id, s);
	}

	/* session may have been freed by now */

	return (0);
}

void
smtp_session_imsg(struct mproc *p, struct imsg *imsg)
{
	struct ca_cert_resp_msg       	*resp_ca_cert;
	struct ca_vrfy_resp_msg       	*resp_ca_vrfy;
	struct smtp_session		*s;
	struct smtp_rcpt		*rcpt;
	void				*ssl;
	char				*pkiname;
	char				 user[SMTPD_MAXLOGNAME];
	struct msg			 m;
	const char			*line, *helo;
	uint64_t			 reqid, evpid;
	uint32_t			 msgid;
	int				 status, success, dnserror;
	X509				*x;
	void				*ssl_ctx;

	switch (imsg->hdr.type) {
	case IMSG_SMTP_DNS_PTR:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &dnserror);
		if (dnserror)
			line = "<unknown>";
		else
			m_get_string(&m, &line);
		m_end(&m);
		s = tree_xpop(&wait_lka_ptr, reqid);
		(void)strlcpy(s->hostname, line, sizeof s->hostname);
		if (smtp_lookup_servername(s))
			smtp_connected(s);
		return;

	case IMSG_SMTP_EXPAND_RCPT:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &status);
		m_get_string(&m, &line);
		m_end(&m);
		s = tree_xpop(&wait_lka_rcpt, reqid);
		switch (status) {
		case LKA_OK:
			fatalx("unexpected ok");
		case LKA_PERMFAIL:
			smtp_reply(s, "%s", line);
			s->rcptfail += 1;
			if (s->rcptfail >= SMTP_KICK_RCPTFAIL) {
				log_info("smtp-in: Ending session %016"PRIx64
				    ": too many failed RCPT", s->id);
				smtp_enter_state(s, STATE_QUIT);
			}
			break;
		case LKA_TEMPFAIL:
			smtp_reply(s, "%s", line);
		}
		io_reload(&s->io);
		return;

	case IMSG_SMTP_LOOKUP_HELO:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		s = tree_xpop(&wait_lka_helo, reqid);
		m_get_int(&m, &status);
		if (status == LKA_OK) {
			m_get_string(&m, &helo);
			(void)strlcpy(s->smtpname, helo, sizeof(s->smtpname));
		}
		m_end(&m);
		smtp_connected(s);
		return;

	case IMSG_SMTP_MESSAGE_CREATE:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &success);
		s = tree_xpop(&wait_queue_msg, reqid);
		if (success) {
			m_get_msgid(&m, &msgid);
			s->evp.id = msgid_to_evpid(msgid);
			s->rcptcount = 0;
			s->phase = PHASE_TRANSACTION;
			smtp_reply(s, "250 %s: Ok",
			    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		} else {
			smtp_reply(s, "421 %s: Temporary Error",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
			smtp_enter_state(s, STATE_QUIT);
		}
		m_end(&m);
		io_reload(&s->io);
		return;

	case IMSG_SMTP_MESSAGE_OPEN:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &success);
		m_end(&m);

		s = tree_xpop(&wait_queue_fd, reqid);
		if (!success || imsg->fd == -1 ||
		    (s->ofile = fdopen(imsg->fd, "w")) == NULL) {
			if (imsg->fd != -1)
				close(imsg->fd);
			smtp_reply(s, "421 %s: Temporary Error",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
			smtp_enter_state(s, STATE_QUIT);
			io_reload(&s->io);
			return;
		}

		fprintf(s->ofile, "Received: ");
		if (! (s->listener->flags & F_MASK_SOURCE)) {
			fprintf(s->ofile, "from %s (%s [%s]);\n\t",
			    s->evp.helo,
			    s->hostname,
			    ss_to_text(&s->ss));
		}
		fprintf(s->ofile, "by %s (%s) with %sSMTP%s%s id %08x;\n",
		    s->smtpname,
		    SMTPD_NAME,
		    s->flags & SF_EHLO ? "E" : "",
		    s->flags & SF_SECURE ? "S" : "",
		    s->flags & SF_AUTHENTICATED ? "A" : "",
		    evpid_to_msgid(s->evp.id));

		if (s->flags & SF_SECURE) {
			x = SSL_get_peer_certificate(s->io.ssl);
			fprintf(s->ofile,
			    "\tTLS version=%s cipher=%s bits=%d verify=%s;\n",
			    SSL_get_cipher_version(s->io.ssl),
			    SSL_get_cipher_name(s->io.ssl),
			    SSL_get_cipher_bits(s->io.ssl, NULL),
			    (s->flags & SF_VERIFIED) ? "YES" : (x ? "FAIL" : "NO"));
			if (x)
				X509_free(x);
		}

		if (s->rcptcount == 1) {
			fprintf(s->ofile, "\tfor <%s@%s>;\n",
			    s->evp.rcpt.user,
			    s->evp.rcpt.domain);
		}

		fprintf(s->ofile, "\t%s\n", time_to_text(time(NULL)));

		smtp_enter_state(s, STATE_BODY);
		smtp_reply(s, "354 Enter mail, end with \".\""
		    " on a line by itself");

		tree_xset(&wait_mfa_data, s->id, s);
		io_reload(&s->io);
		return;

	case IMSG_QUEUE_ENVELOPE_SUBMIT:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &success);
		s = tree_xget(&wait_lka_rcpt, reqid);
		if (success) {
			m_get_evpid(&m, &evpid);
			s->destcount++;
		}
		else
			s->msgflags |= MF_QUEUE_ENVELOPE_FAIL;
		m_end(&m);
		return;

	case IMSG_QUEUE_ENVELOPE_COMMIT:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &success);
		m_end(&m);
		if (!success)
			fatalx("commit evp failed: not supposed to happen");
		s = tree_xpop(&wait_lka_rcpt, reqid);
		if (s->msgflags & MF_QUEUE_ENVELOPE_FAIL) {
			/*
			 * If an envelope failed, we can't cancel the last
			 * RCPT only so we must cancel the whole transaction
			 * and close the connection.
			 */
			smtp_reply(s, "421 %s: Temporary failure",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
			smtp_enter_state(s, STATE_QUIT);
		}
		else {
			rcpt = xcalloc(1, sizeof(*rcpt), "smtp_rcpt");
			rcpt->destcount = s->destcount;
			rcpt->maddr = s->evp.rcpt;
			TAILQ_INSERT_TAIL(&s->rcpts, rcpt, entry);

			s->destcount = 0;
			s->rcptcount++;
			s->kickcount--;
			smtp_reply(s, "250 %s %s: Recipient ok",
			    esc_code(ESC_STATUS_OK, ESC_DESTINATION_ADDRESS_VALID),
			    esc_description(ESC_DESTINATION_ADDRESS_VALID));
		}
		io_reload(&s->io);
		return;

	case IMSG_SMTP_MESSAGE_COMMIT:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &success);
		m_end(&m);
		s = tree_xpop(&wait_queue_commit, reqid);
		if (!success) {
			smtp_filter_rollback(s);
			smtp_reply(s, "421 %s: Temporary failure",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
			smtp_enter_state(s, STATE_QUIT);
			io_reload(&s->io);
			return;
		}

		smtp_filter_commit(s);
		smtp_reply(s, "250 %s: %08x Message accepted for delivery",
		    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS),
		    evpid_to_msgid(s->evp.id));

		TAILQ_FOREACH(rcpt, &s->rcpts, entry) {
			log_info("smtp-in: Accepted message %08x "
			    "on session %016"PRIx64
			    ": from=<%s%s%s>, to=<%s%s%s>, size=%zu, ndest=%zu, proto=%s",
			    evpid_to_msgid(s->evp.id),
			    s->id,
			    s->evp.sender.user,
			    s->evp.sender.user[0] == '\0' ? "" : "@",
			    s->evp.sender.domain,
			    rcpt->maddr.user,
			    rcpt->maddr.user[0] == '\0' ? "" : "@",
			    rcpt->maddr.domain,
			    s->datalen,
			    rcpt->destcount,
			    s->flags & SF_EHLO ? "ESMTP" : "SMTP");
		}

		s->mailcount++;
		s->kickcount = 0;
		s->phase = PHASE_SETUP;
		smtp_message_reset(s, 0);
		smtp_enter_state(s, STATE_HELO);
		io_reload(&s->io);
		return;

	case IMSG_SMTP_AUTHENTICATE:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &success);
		m_end(&m);

		s = tree_xpop(&wait_parent_auth, reqid);
		strnvis(user, s->username, sizeof user, VIS_WHITE | VIS_SAFE);
		if (success == LKA_OK) {
			log_info("smtp-in: Accepted authentication for user %s "
			    "on session %016"PRIx64, user, s->id);
			s->kickcount = 0;
			s->flags |= SF_AUTHENTICATED;
			smtp_reply(s, "235 %s: Authentication succeeded",
			    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		}
		else if (success == LKA_PERMFAIL) {
			log_info("smtp-in: Authentication failed for user %s "
			    "on session %016"PRIx64, user, s->id);
			smtp_auth_failure_pause(s);
			return;
		}
		else if (success == LKA_TEMPFAIL) {
			log_info("smtp-in: Authentication temporarily failed "
			    "for user %s on session %016"PRIx64, user, s->id);
			smtp_reply(s, "421 %s: Temporary failure",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
		}
		else
			fatalx("bad lka response");

		smtp_enter_state(s, STATE_HELO);
		io_reload(&s->io);
		return;

	case IMSG_SMTP_SSL_INIT:
		resp_ca_cert = imsg->data;
		s = tree_xpop(&wait_ssl_init, resp_ca_cert->reqid);

		if (resp_ca_cert->status == CA_FAIL) {
			log_info("smtp-in: Disconnecting session %016" PRIx64
			    ": CA failure", s->id);
			smtp_free(s, "CA failure");
			return;
		}

		resp_ca_cert = xmemdup(imsg->data, sizeof *resp_ca_cert, "smtp:ca_cert");
		if (resp_ca_cert == NULL)
			fatal(NULL);
		resp_ca_cert->cert = xstrdup((char *)imsg->data +
		    sizeof *resp_ca_cert, "smtp:ca_cert");
		if (s->listener->pki_name[0])
			pkiname = s->listener->pki_name;
		else
			pkiname = s->smtpname;
		ssl_ctx = dict_get(env->sc_ssl_dict, pkiname);

		ssl = ssl_smtp_init(ssl_ctx, smtp_sni_callback, s);
		io_set_read(&s->io);
		io_start_tls(&s->io, ssl);

		explicit_bzero(resp_ca_cert->cert, resp_ca_cert->cert_len);
		free(resp_ca_cert->cert);
		free(resp_ca_cert);
		return;

	case IMSG_SMTP_SSL_VERIFY:
		resp_ca_vrfy = imsg->data;
		s = tree_xpop(&wait_ssl_verify, resp_ca_vrfy->reqid);

		if (resp_ca_vrfy->status == CA_OK)
			s->flags |= SF_VERIFIED;
		else if (s->listener->flags & F_TLS_VERIFY) {
			log_info("smtp-in: Disconnecting session %016" PRIx64
			    ": SSL certificate check failed", s->id);
			smtp_free(s, "SSL certificate check failed");	
			return;
		}
		smtp_io(&s->io, IO_TLSVERIFIED);
		io_resume(&s->io, IO_PAUSE_IN);
		return;
	}

	log_warnx("smtp_session_imsg: unexpected %s imsg",
	    imsg_to_str(imsg->hdr.type));
	fatalx(NULL);
}

static void
smtp_mfa_response(struct smtp_session *s, int msg, int status, uint32_t code,
    const char *line)
{
	struct ca_cert_req_msg		 req_ca_cert;

	if (status == FILTER_CLOSE) {
		code = code ? code : 421;
		line = line ? line : "Temporary failure";
		smtp_reply(s, "%d %s", code, line);
		smtp_enter_state(s, STATE_QUIT);
		io_reload(&s->io);
		return;
	}

	switch (msg) {

	case IMSG_SMTP_REQ_CONNECT:
		if (status != FILTER_OK) {
			log_info("smtp-in: Disconnecting session %016" PRIx64
			    ": rejected by filter", s->id);
			smtp_free(s, "rejected by filter");
			return;
		}

		if (s->listener->flags & F_SMTPS) {
			req_ca_cert.reqid = s->id;
			if (s->listener->pki_name[0])
				(void)strlcpy(req_ca_cert.name, s->listener->pki_name,
				    sizeof req_ca_cert.name);
			else
				(void)strlcpy(req_ca_cert.name, s->smtpname,
				    sizeof req_ca_cert.name);
			m_compose(p_lka, IMSG_SMTP_SSL_INIT, 0, 0, -1,
			    &req_ca_cert, sizeof(req_ca_cert));
			tree_xset(&wait_ssl_init, s->id, s);
			return;
		}
		smtp_send_banner(s);
		return;

	case IMSG_SMTP_REQ_HELO:
		if (status != FILTER_OK) {
			code = code ? code : 530;
			line = line ? line : "Hello rejected";
			smtp_reply(s, "%d %s", code, line);
			io_reload(&s->io);
			return;
		}

		smtp_enter_state(s, STATE_HELO);
		smtp_reply(s, "250%c%s Hello %s [%s], pleased to meet you",
		    (s->flags & SF_EHLO) ? '-' : ' ',
		    s->smtpname,
		    s->evp.helo,
		    ss_to_text(&s->ss));

		if (s->flags & SF_EHLO) {
			smtp_reply(s, "250-8BITMIME");
			smtp_reply(s, "250-ENHANCEDSTATUSCODES");
			smtp_reply(s, "250-SIZE %zu", env->sc_maxsize);
			if (ADVERTISE_EXT_DSN(s))
				smtp_reply(s, "250-DSN");
			if (ADVERTISE_TLS(s))
				smtp_reply(s, "250-STARTTLS");
			if (ADVERTISE_AUTH(s))
				smtp_reply(s, "250-AUTH PLAIN LOGIN");
			smtp_reply(s, "250 HELP");
		}
		s->kickcount = 0;
		s->phase = PHASE_SETUP;
		io_reload(&s->io);
		return;

	case IMSG_SMTP_REQ_MAIL:
		if (status != FILTER_OK) {
			code = code ? code : 530;
			line = line ? line : "Sender rejected";
			smtp_reply(s, "%d %s", code, line);
			io_reload(&s->io);
			return;
		}

		m_create(p_queue, IMSG_SMTP_MESSAGE_CREATE, 0, 0, -1);
		m_add_id(p_queue, s->id);
		m_close(p_queue);
		tree_xset(&wait_queue_msg, s->id, s);
		return;

	case IMSG_SMTP_REQ_RCPT:
		if (status != FILTER_OK) {
			code = code ? code : 530;
			line = line ? line : "Recipient rejected";
			smtp_reply(s, "%d %s", code, line);

			s->rcptfail += 1;
			if (s->rcptfail >= SMTP_KICK_RCPTFAIL) {
				log_info("smtp-in: Ending session %016" PRIx64
				    ": too many failed RCPT", s->id);
				smtp_enter_state(s, STATE_QUIT);
			}
			io_reload(&s->io);
			return;
		}

		m_create(p_lka, IMSG_SMTP_EXPAND_RCPT, 0, 0, -1);
		m_add_id(p_lka, s->id);
		m_add_envelope(p_lka, &s->evp);
		m_close(p_lka);
		tree_xset(&wait_lka_rcpt, s->id, s);
		return;

	case IMSG_SMTP_REQ_DATA:
		if (status != FILTER_OK) {
			code = code ? code : 530;
			line = line ? line : "Message rejected";
			smtp_reply(s, "%d %s", code, line);
			io_reload(&s->io);
			return;
		}
		m_create(p_queue, IMSG_SMTP_MESSAGE_OPEN, 0, 0, -1);
		m_add_id(p_queue, s->id);
		m_add_msgid(p_queue, evpid_to_msgid(s->evp.id));
		m_close(p_queue);
		tree_xset(&wait_queue_fd, s->id, s);
		return;

	case IMSG_SMTP_REQ_EOM:
		if (status != FILTER_OK) {
			code = code ? code : 530;
			line = line ? line : "Message rejected";
			smtp_reply(s, "%d %s", code, line);
			io_reload(&s->io);
			return;
		}
		smtp_message_end(s);
		return;

	default:
		fatal("bad mfa_imsg");
	}
}

static void
smtp_io(struct io *io, int evt)
{
	struct ca_cert_req_msg	req_ca_cert;
	struct smtp_session    *s = io->arg;
	char		       *line;
	size_t			len, i;
	X509		       *x;

	log_trace(TRACE_IO, "smtp: %p: %s %s", s, io_strevent(evt),
	    io_strio(io));

	switch (evt) {

	case IO_TLSREADY:
		log_info("smtp-in: Started TLS on session %016"PRIx64": %s",
		    s->id, ssl_to_text(s->io.ssl));

		s->flags |= SF_SECURE;
		s->kickcount = 0;
		s->phase = PHASE_INIT;

		if (smtp_verify_certificate(s)) {
			io_pause(&s->io, IO_PAUSE_IN);
			break;
		}

		if (s->listener->flags & F_TLS_VERIFY) {
			log_info("smtp-in: Disconnecting session %016" PRIx64
			    ": client did not present certificate", s->id);
			smtp_free(s, "client did not present certificate");	
			return;
		}

		/* No verification required, cascade */

	case IO_TLSVERIFIED:
		x = SSL_get_peer_certificate(s->io.ssl);
		if (x) {
			log_info("smtp-in: Client certificate verification %s "
			    "on session %016"PRIx64,
			    (s->flags & SF_VERIFIED) ? "succeeded" : "failed",
			    s->id);
			X509_free(x);
		}

		if (s->listener->flags & F_SMTPS) {
			stat_increment("smtp.smtps", 1);
			io_set_write(&s->io);
			smtp_send_banner(s);
		}
		else {
			stat_increment("smtp.tls", 1);
			smtp_enter_state(s, STATE_HELO);
		}
		break;

	case IO_DATAIN:
	    nextline:
		line = iobuf_getline(&s->iobuf, &len);
		if ((line == NULL && iobuf_len(&s->iobuf) >= SMTPD_MAXLINESIZE) ||
		    (line && len >= SMTPD_MAXLINESIZE)) {
			s->flags |= SF_BADINPUT;
			smtp_reply(s, "500 %s: Line too long",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_OTHER_STATUS));
			smtp_enter_state(s, STATE_QUIT);
			io_set_write(io);
			return;
		}

		/* No complete line received */
		if (line == NULL) {
			iobuf_normalize(&s->iobuf);
			return;
		}

		/* Message body */
		if (s->state == STATE_BODY && strcmp(line, ".")) {

			if (line[0] == '.') {
				line += 1;
				len -= 1;
			}

			if (!(s->flags & SF_8BITMIME))
				for (i = 0; i < len; ++i)
					if (line[i] & 0x80)
						line[i] = line[i] & 0x7f;

			smtp_message_write(s, line);
			goto nextline;
		}

		/* Pipelining not supported */
		if (iobuf_len(&s->iobuf)) {
			s->flags |= SF_BADINPUT;
			smtp_reply(s, "500 %s %s: Pipelining not supported",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			smtp_enter_state(s, STATE_QUIT);
			io_set_write(io);
			return;
		}

		/* End of body */
		if (s->state == STATE_BODY) {
			iobuf_normalize(&s->iobuf);
			io_set_write(io);

			smtp_filter_eom(s);
			return;
		}

		/* Must be a command */
		(void)strlcpy(s->cmd, line, sizeof s->cmd);
		io_set_write(io);
		smtp_command(s, line);
		iobuf_normalize(&s->iobuf);
		if (s->flags & SF_KICK)
			smtp_free(s, "kick");
		break;

	case IO_LOWAT:
		if (s->state == STATE_QUIT) {
			log_info("smtp-in: Closing session %016" PRIx64, s->id);
			smtp_free(s, "done");
			break;
		}

		/* Wait for the client to start tls */
		if (s->state == STATE_TLS) {
			req_ca_cert.reqid = s->id;
			if (s->listener->pki_name[0])
				(void)strlcpy(req_ca_cert.name, s->listener->pki_name,
				    sizeof req_ca_cert.name);
			else
				(void)strlcpy(req_ca_cert.name, s->smtpname,
				    sizeof req_ca_cert.name);
			m_compose(p_lka, IMSG_SMTP_SSL_INIT, 0, 0, -1,
			    &req_ca_cert, sizeof(req_ca_cert));
			tree_xset(&wait_ssl_init, s->id, s);
			break;
		}

		io_set_read(io);
		break;

	case IO_TIMEOUT:
		log_info("smtp-in: Disconnecting session %016"PRIx64": "
		    "session timeout", s->id);
		smtp_free(s, "timeout");
		break;

	case IO_DISCONNECTED:
		log_info("smtp-in: Received disconnect from session %016"PRIx64,
		    s->id);
		smtp_free(s, "disconnected");
		break;

	case IO_ERROR:
		log_info("smtp-in: Disconnecting session %016"PRIx64": "
		    "IO error: %s", s->id, io->error);
		smtp_free(s, "IO error");
		break;

	default:
		fatalx("smtp_io()");
	}
}

static void
smtp_command(struct smtp_session *s, char *line)
{
	char			       *args, *eom, *method;
	int				cmd, i;

	log_trace(TRACE_SMTP, "smtp: %p: <<< %s", s, line);

	if (++s->kickcount >= SMTP_KICK_CMD) {
		log_info("smtp-in: Disconnecting session %016" PRIx64
		    ": session not moving forward", s->id);
		s->flags |= SF_KICK;
		stat_increment("smtp.kick", 1);
		return;
	}

	/*
	 * These states are special.
	 */
	if (s->state == STATE_AUTH_INIT) {
		smtp_rfc4954_auth_plain(s, line);
		return;
	}
	if (s->state == STATE_AUTH_USERNAME || s->state == STATE_AUTH_PASSWORD) {
		smtp_rfc4954_auth_login(s, line);
		return;
	}

	/*
	 * Unlike other commands, "mail from" and "rcpt to" contain a
	 * space in the command name.
	 */
	if (strncasecmp("mail from:", line, 10) == 0 ||
	    strncasecmp("rcpt to:", line, 8) == 0)
		args = strchr(line, ':');
	else
		args = strchr(line, ' ');

	if (args) {
		*args++ = '\0';
		while (isspace((unsigned char)*args))
			args++;
	}

	cmd = -1;
	for (i = 0; commands[i].code != -1; i++)
		if (!strcasecmp(line, commands[i].cmd)) {
			cmd = commands[i].code;
			break;
		}

	switch (cmd) {
	/*
	 * INIT
	 */
	case CMD_HELO:
	case CMD_EHLO:
		if (s->phase != PHASE_INIT) {
			smtp_reply(s, "503 %s %s: Already identified",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (args == NULL) {
			smtp_reply(s, "501 %s %s: %s requires domain name",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND),
			    (cmd == CMD_HELO) ? "HELO" : "EHLO");

			break;
		}

		if (!valid_domainpart(args)) {
			smtp_reply(s, "501 %s %s: Invalid domain name",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND_ARGUMENTS),
			    esc_description(ESC_INVALID_COMMAND_ARGUMENTS));
			break;
		}
		(void)strlcpy(s->helo, args, sizeof(s->helo));
		s->flags &= SF_SECURE | SF_AUTHENTICATED | SF_VERIFIED;
		if (cmd == CMD_EHLO) {
			s->flags |= SF_EHLO;
			s->flags |= SF_8BITMIME;
		}

		smtp_message_reset(s, 1);

		smtp_filter_helo(s);
		break;
	/*
	 * SETUP
	 */
	case CMD_STARTTLS:
		if (s->phase != PHASE_SETUP) {
			smtp_reply(s, "503 %s %s: Command not allowed at this point.",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (!(s->listener->flags & F_STARTTLS)) {
			smtp_reply(s, "503 %s %s: Command not supported",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (s->flags & SF_SECURE) {
			smtp_reply(s, "503 %s %s: Channel already secured",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}
		if (args != NULL) {
			smtp_reply(s, "501 %s %s: No parameters allowed",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND_ARGUMENTS),
			    esc_description(ESC_INVALID_COMMAND_ARGUMENTS));
			break;
		}
		smtp_reply(s, "220 %s: Ready to start TLS",
		    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		smtp_enter_state(s, STATE_TLS);
		break;

	case CMD_AUTH:
		if (s->phase != PHASE_SETUP) {
			smtp_reply(s, "503 %s %s: Command not allowed at this point.",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (s->flags & SF_AUTHENTICATED) {
			smtp_reply(s, "503 %s %s: Already authenticated",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (!ADVERTISE_AUTH(s)) {
			smtp_reply(s, "503 %s %s: Command not supported",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (args == NULL) {
			smtp_reply(s, "501 %s %s: No parameters given",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND_ARGUMENTS),
			    esc_description(ESC_INVALID_COMMAND_ARGUMENTS));
			break;
		}

		method = args;
		eom = strchr(args, ' ');
		if (eom == NULL)
			eom = strchr(args, '\t');
		if (eom != NULL)
			*eom++ = '\0';
		if (strcasecmp(method, "PLAIN") == 0)
			smtp_rfc4954_auth_plain(s, eom);
		else if (strcasecmp(method, "LOGIN") == 0)
			smtp_rfc4954_auth_login(s, eom);
		else
			smtp_reply(s, "504 %s %s: AUTH method \"%s\" not supported",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_SECURITY_FEATURES_NOT_SUPPORTED),
			    esc_description(ESC_SECURITY_FEATURES_NOT_SUPPORTED),
			    method);
		break;

	case CMD_MAIL_FROM:
		if (s->phase != PHASE_SETUP) {
			smtp_reply(s, "503 %s %s: Command not allowed at this point.",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));

			break;
		}

		if (s->listener->flags & F_STARTTLS_REQUIRE &&
		    !(s->flags & SF_SECURE)) {
			smtp_reply(s,
			    "530 %s %s: Must issue a STARTTLS command first",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (s->listener->flags & F_AUTH_REQUIRE &&
		    !(s->flags & SF_AUTHENTICATED)) {
			smtp_reply(s,
			    "530 %s %s: Must issue an AUTH command first",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (s->mailcount >= SMTP_LIMIT_MAIL) {
			/* we can pretend we had too many recipients */
			smtp_reply(s, "452 %s %s: Too many messages sent",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_TOO_MANY_RECIPIENTS),
			    esc_description(ESC_TOO_MANY_RECIPIENTS));
			break;
		}

		smtp_message_reset(s, 1);

		if (smtp_mailaddr(&s->evp.sender, args, 1, &args,
			s->smtpname) == 0) {
			smtp_reply(s, "553 %s: Sender address syntax error",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_OTHER_ADDRESS_STATUS));
			break;
		}
		if (args && smtp_parse_mail_args(s, args) == -1)
			break;

		smtp_filter_mail(s);
		break;
	/*
	 * TRANSACTION
	 */
	case CMD_RCPT_TO:
		if (s->phase != PHASE_TRANSACTION) {
			smtp_reply(s, "503 %s %s: Command not allowed at this point.",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (s->rcptcount >= SMTP_LIMIT_RCPT) {
			smtp_reply(s, "451 %s %s: Too many recipients",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_TOO_MANY_RECIPIENTS),
			    esc_description(ESC_TOO_MANY_RECIPIENTS));
			break;
		}

		if (smtp_mailaddr(&s->evp.rcpt, args, 0, &args,
		    s->smtpname) == 0) {
			smtp_reply(s,
			    "501 %s: Recipient address syntax error",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_BAD_DESTINATION_MAILBOX_ADDRESS_SYNTAX));
			break;
		}
		if (args && smtp_parse_rcpt_args(s, args) == -1)
			break;

		smtp_filter_rcpt(s);
		break;

	case CMD_RSET:
		if (s->phase != PHASE_TRANSACTION && s->phase != PHASE_SETUP) {
			smtp_reply(s, "503 %s %s: Command not allowed at this point.",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		smtp_filter_rset(s);

		if (s->evp.id) {
			m_create(p_queue, IMSG_SMTP_MESSAGE_ROLLBACK, 0, 0, -1);
			m_add_msgid(p_queue, evpid_to_msgid(s->evp.id));
			m_close(p_queue);
		}

		s->phase = PHASE_SETUP;
		smtp_message_reset(s, 0);
		smtp_reply(s, "250 %s: Reset state",
		    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		break;

	case CMD_DATA:
		if (s->phase != PHASE_TRANSACTION) {
			smtp_reply(s, "503 %s %s: Command not allowed at this point.",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}
		if (s->rcptcount == 0) {
			smtp_reply(s, "503 %s %s: No recipient specified",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND_ARGUMENTS),
			    esc_description(ESC_INVALID_COMMAND_ARGUMENTS));
			break;
		}

		rfc2822_parser_reset(&s->rfc2822_parser);

		smtp_filter_data(s);
		break;
	/*
	 * ANY
	 */
	case CMD_QUIT:
		smtp_reply(s, "221 %s: Bye",
		    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		smtp_enter_state(s, STATE_QUIT);
		break;

	case CMD_NOOP:
		smtp_reply(s, "250 %s: Ok",
		    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		break;

	case CMD_HELP:
		smtp_reply(s, "214- This is OpenSMTPD");
		smtp_reply(s, "214- To report bugs in the implementation, "
		    "please contact bugs@openbsd.org");
		smtp_reply(s, "214- with full details");
		smtp_reply(s, "214 %s: End of HELP info",
		    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		break;

	case CMD_WIZ:
		smtp_reply(s, "500 %s %s: this feature is not supported yet ;-)",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
		break;

	default:
		smtp_reply(s, "500 %s %s: Command unrecognized",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
		break;
	}
}

static void
smtp_rfc4954_auth_plain(struct smtp_session *s, char *arg)
{
	char		 buf[1024], *user, *pass;
	int		 len;

	switch (s->state) {
	case STATE_HELO:
		if (arg == NULL) {
			smtp_enter_state(s, STATE_AUTH_INIT);
			smtp_reply(s, "334 ");
			return;
		}
		smtp_enter_state(s, STATE_AUTH_INIT);
		/* FALLTHROUGH */

	case STATE_AUTH_INIT:
		/* String is not NUL terminated, leave room. */
		if ((len = base64_decode(arg, (unsigned char *)buf,
			    sizeof(buf) - 1)) == -1)
			goto abort;
		/* buf is a byte string, NUL terminate. */
		buf[len] = '\0';

		/*
		 * Skip "foo" in "foo\0user\0pass", if present.
		 */
		user = memchr(buf, '\0', len);
		if (user == NULL || user >= buf + len - 2)
			goto abort;
		user++; /* skip NUL */
		if (strlcpy(s->username, user, sizeof(s->username))
		    >= sizeof(s->username))
			goto abort;

		pass = memchr(user, '\0', len - (user - buf));
		if (pass == NULL || pass >= buf + len - 2)
			goto abort;
		pass++; /* skip NUL */

		m_create(p_lka,  IMSG_SMTP_AUTHENTICATE, 0, 0, -1);
		m_add_id(p_lka, s->id);
		m_add_string(p_lka, s->listener->authtable);
		m_add_string(p_lka, user);
		m_add_string(p_lka, pass);
		m_close(p_lka);
		tree_xset(&wait_parent_auth, s->id, s);
		return;

	default:
		fatal("smtp_rfc4954_auth_plain: unknown state");
	}

abort:
	smtp_reply(s, "501 %s %s: Syntax error",
	    esc_code(ESC_STATUS_PERMFAIL, ESC_SYNTAX_ERROR),
	    esc_description(ESC_SYNTAX_ERROR));
	smtp_enter_state(s, STATE_HELO);
}

static void
smtp_rfc4954_auth_login(struct smtp_session *s, char *arg)
{
	char		buf[SMTPD_MAXLINESIZE];

	switch (s->state) {
	case STATE_HELO:
		smtp_enter_state(s, STATE_AUTH_USERNAME);
		smtp_reply(s, "334 VXNlcm5hbWU6");
		return;

	case STATE_AUTH_USERNAME:
		memset(s->username, 0, sizeof(s->username));
		if (base64_decode(arg, (unsigned char *)s->username,
				  sizeof(s->username) - 1) == -1)
			goto abort;

		smtp_enter_state(s, STATE_AUTH_PASSWORD);
		smtp_reply(s, "334 UGFzc3dvcmQ6");
		return;

	case STATE_AUTH_PASSWORD:
		memset(buf, 0, sizeof(buf));
		if (base64_decode(arg, (unsigned char *)buf,
				  sizeof(buf)-1) == -1)
			goto abort;

		m_create(p_lka,  IMSG_SMTP_AUTHENTICATE, 0, 0, -1);
		m_add_id(p_lka, s->id);
		m_add_string(p_lka, s->listener->authtable);
		m_add_string(p_lka, s->username);
		m_add_string(p_lka, buf);
		m_close(p_lka);
		tree_xset(&wait_parent_auth, s->id, s);
		return;

	default:
		fatal("smtp_rfc4954_auth_login: unknown state");
	}

abort:
	smtp_reply(s, "501 %s %s: Syntax error",
	    esc_code(ESC_STATUS_PERMFAIL, ESC_SYNTAX_ERROR),
	    esc_description(ESC_SYNTAX_ERROR));
	smtp_enter_state(s, STATE_HELO);
}

static uint8_t
dsn_notify_str_to_uint8(const char *arg)
{
	if (strcasecmp(arg, "SUCCESS") == 0)
		return DSN_SUCCESS;
	else if (strcasecmp(arg, "FAILURE") == 0)
		return DSN_FAILURE;
	else if (strcasecmp(arg, "DELAY") == 0)
		return DSN_DELAY;
	else if (strcasecmp(arg, "NEVER") == 0)
		return DSN_NEVER;

	return (0);
}

static int
smtp_parse_rcpt_args(struct smtp_session *s, char *args)
{
	char 	*b, *p;
	uint8_t flag;

	while ((b = strsep(&args, " "))) {
		if (*b == '\0')
			continue;
		
		if (ADVERTISE_EXT_DSN(s) && strncasecmp(b, "NOTIFY=", 7) == 0) {
			b += 7;
			while ((p = strsep(&b, ","))) {
				if (*p == '\0')
					continue;

				if ((flag = dsn_notify_str_to_uint8(p)) == 0)
					continue;

				s->evp.dsn_notify |= flag;
			}
			if (s->evp.dsn_notify & DSN_NEVER &&
			    s->evp.dsn_notify & (DSN_SUCCESS | DSN_FAILURE |
			    DSN_DELAY)) {
				smtp_reply(s,
				    "553 NOTIFY option NEVER cannot be \
				    combined with other options");
				return (-1);
			}
		} else if (ADVERTISE_EXT_DSN(s) && strncasecmp(b, "ORCPT=", 6) == 0) {
			b += 6;
			if (!text_to_mailaddr(&s->evp.dsn_orcpt, b)) {
				smtp_reply(s, "553 ORCPT address syntax error");
				return (-1);
			}
		} else {
			smtp_reply(s, "503 Unsupported option %s", b);
			return (-1);
		}
	}

	return (0);
}

static int
smtp_parse_mail_args(struct smtp_session *s, char *args)
{
	char *b;

	while ((b = strsep(&args, " "))) {
		if (*b == '\0')
			continue;

		if (strncasecmp(b, "AUTH=", 5) == 0)
			log_debug("debug: smtp: AUTH in MAIL FROM command");
		else if (strncasecmp(b, "SIZE=", 5) == 0)
			log_debug("debug: smtp: SIZE in MAIL FROM command");
		else if (strcasecmp(b, "BODY=7BIT") == 0)
			/* XXX only for this transaction */
			s->flags &= ~SF_8BITMIME;
		else if (strcasecmp(b, "BODY=8BITMIME") == 0)
			;
		else if (ADVERTISE_EXT_DSN(s) && strncasecmp(b, "RET=", 4) == 0) {
			b += 4;
			if (strcasecmp(b, "HDRS") == 0)
				s->evp.dsn_ret = DSN_RETHDRS;
			else if (strcasecmp(b, "FULL") == 0)
				s->evp.dsn_ret = DSN_RETFULL;
		} else if (ADVERTISE_EXT_DSN(s) && strncasecmp(b, "ENVID=", 6) == 0) {
			b += 6;
			if (strlcpy(s->evp.dsn_envid, b, sizeof(s->evp.dsn_envid))
			    >= sizeof(s->evp.dsn_envid)) {
				smtp_reply(s, "503 %s %s: option too large, truncated: %s",
				    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND_ARGUMENTS),
				    esc_description(ESC_INVALID_COMMAND_ARGUMENTS), b);
				return (-1);
			}
		} else {
			smtp_reply(s, "503 %s %s: Unsupported option %s",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND_ARGUMENTS),
			    esc_description(ESC_INVALID_COMMAND_ARGUMENTS), b);
			return (-1);
		}
	}

	return (0);
}

static int
smtp_lookup_servername(struct smtp_session *s)
{
	struct sockaddr		*sa;
	socklen_t		 sa_len;
	struct sockaddr_storage	 ss;

	if (s->listener->hostnametable[0]) {
		sa_len = sizeof(ss);
		sa = (struct sockaddr *)&ss;
		if (getsockname(s->io.sock, sa, &sa_len) == -1) {
			log_warn("warn: getsockname()");
		}
		else {
			m_create(p_lka, IMSG_SMTP_LOOKUP_HELO, 0, 0, -1);
			m_add_id(p_lka, s->id);
			m_add_string(p_lka, s->listener->hostnametable);
			m_add_sockaddr(p_lka, sa);
			m_close(p_lka);
			tree_xset(&wait_lka_helo, s->id, s);
			return 0;
		}
	}
	return 1;
}

static void
smtp_connected(struct smtp_session *s)
{
	struct sockaddr_storage	ss;
	socklen_t		sl;

	smtp_enter_state(s, STATE_CONNECTED);

	log_info("smtp-in: New session %016"PRIx64" from host %s [%s]",
	    s->id, s->hostname, ss_to_text(&s->ss));

	sl = sizeof(ss);
	if (getsockname(s->io.sock, (struct sockaddr*)&ss, &sl) == -1) {
		smtp_free(s, strerror(errno));
		return;
	}

	s->flags |= SF_MFACONNSENT;
	smtp_filter_connect(s, (struct sockaddr *)&ss);
}

static void
smtp_send_banner(struct smtp_session *s)
{
	smtp_reply(s, "220 %s ESMTP %s", s->smtpname, SMTPD_NAME);
	io_reload(&s->io);
}

void
smtp_enter_state(struct smtp_session *s, int newstate)
{
	log_trace(TRACE_SMTP, "smtp: %p: %s -> %s", s,
	    smtp_strstate(s->state),
	    smtp_strstate(newstate));

	s->state = newstate;
}

static void
smtp_message_write(struct smtp_session *s, const char *line)
{
	int	ret;

	log_trace(TRACE_SMTP, "<<< [MSG] %s", line);

	/* Don't waste resources on message if it's going to bin anyway. */
	if (s->msgflags & MF_ERROR)
		return;

	if (*line == '\0')
		s->hdrdone = 1;

	/* check for loops */
	if (!s->hdrdone) {
		if (strncasecmp("Received: ", line, 10) == 0)
			s->rcvcount++;
		if (s->rcvcount == MAX_HOPS_COUNT) {
			s->msgflags |= MF_ERROR_LOOP;
			log_warnx("warn: loop detected");
			return;
		}
	}

	ret = rfc2822_parser_feed(&s->rfc2822_parser, line);
	if (ret == -1) {
		s->msgflags |= MF_ERROR_RESOURCES;
		return;
		
	}
	if (ret == 0) {
		s->msgflags |= MF_ERROR_MALFORMED;
		return;
	}
}

static void
smtp_message_end(struct smtp_session *s)
{
	log_debug("debug: %p: end of message, msgflags=0x%04x", s, s->msgflags);

	tree_xpop(&wait_mfa_data, s->id);

	s->phase = PHASE_SETUP;

	fclose(s->ofile);
	s->ofile = NULL;

	if (s->msgflags & MF_ERROR) {
		m_create(p_queue, IMSG_SMTP_MESSAGE_ROLLBACK, 0, 0, -1);
		m_add_msgid(p_queue, evpid_to_msgid(s->evp.id));
		m_close(p_queue);
		if (s->msgflags & MF_ERROR_SIZE)
			smtp_reply(s, "554 %s %s: Transaction failed, message too big",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_MESSAGE_TOO_BIG_FOR_SYSTEM),
			    esc_description(ESC_MESSAGE_TOO_BIG_FOR_SYSTEM));
		else if (s->msgflags & MF_ERROR_LOOP)
			smtp_reply(s, "500 %s %s: Loop detected",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_ROUTING_LOOP_DETECTED),
			    esc_description(ESC_ROUTING_LOOP_DETECTED));
		else if (s->msgflags & MF_ERROR_RESOURCES)
			smtp_reply(s, "421 %s: Temporary Error",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
		else if (s->msgflags & MF_ERROR_MALFORMED)
			smtp_reply(s, "550 %s %s: Message is not RFC 2822 compliant",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_DELIVERY_NOT_AUTHORIZED_MESSAGE_REFUSED),
			    esc_description(ESC_DELIVERY_NOT_AUTHORIZED_MESSAGE_REFUSED));
		else
			smtp_reply(s, "%d Message rejected", s->msgcode);
		smtp_message_reset(s, 0);
		smtp_enter_state(s, STATE_HELO);
		return;
	}

	m_create(p_queue, IMSG_SMTP_MESSAGE_COMMIT, 0, 0, -1);
	m_add_id(p_queue, s->id);
	m_add_msgid(p_queue, evpid_to_msgid(s->evp.id));
	m_close(p_queue);
	tree_xset(&wait_queue_commit, s->id, s);
}

static void
smtp_message_reset(struct smtp_session *s, int prepare)
{
	struct smtp_rcpt	*rcpt;

	while ((rcpt = TAILQ_FIRST(&s->rcpts))) {
		TAILQ_REMOVE(&s->rcpts, rcpt, entry);
		free(rcpt);
	}

	memset(&s->evp, 0, sizeof s->evp);
	s->msgflags = 0;
	s->destcount = 0;
	s->rcptcount = 0;
	s->datalen = 0;
	s->rcvcount = 0;
	s->hdrdone = 0;

	if (prepare) {
		s->evp.ss = s->ss;
		(void)strlcpy(s->evp.tag, s->listener->tag, sizeof(s->evp.tag));
		(void)strlcpy(s->evp.smtpname, s->smtpname, sizeof(s->evp.smtpname));
		(void)strlcpy(s->evp.hostname, s->hostname, sizeof s->evp.hostname);
		(void)strlcpy(s->evp.helo, s->helo, sizeof s->evp.helo);

		if (s->flags & SF_BOUNCE)
			s->evp.flags |= EF_BOUNCE;
		if (s->flags & SF_AUTHENTICATED)
			s->evp.flags |= EF_AUTHENTICATED;
	}
}

static void
smtp_reply(struct smtp_session *s, char *fmt, ...)
{
	va_list	 ap;
	int	 n;
	char	 buf[SMTPD_MAXLINESIZE], tmp[SMTPD_MAXLINESIZE];

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (n == -1 || n >= SMTPD_MAXLINESIZE)
		fatalx("smtp_reply: line too long");
	if (n < 4)
		fatalx("smtp_reply: response too short");

	log_trace(TRACE_SMTP, "smtp: %p: >>> %s", s, buf);

	iobuf_xfqueue(&s->iobuf, "smtp_reply", "%s\r\n", buf);

	switch (buf[0]) {
	case '5':
	case '4':
		if (s->flags & SF_BADINPUT) {
			log_info("smtp-in: Bad input on session %016"PRIx64
			    ": %.*s", s->id, n, buf);
		}
		else if (strstr(s->cmd, "AUTH ") == s->cmd) {
			log_info("smtp-in: Failed command on session %016"PRIx64
			    ": \"AUTH [...]\" => %.*s", s->id, n, buf);
		}
		else {
			strnvis(tmp, s->cmd, sizeof tmp, VIS_SAFE | VIS_CSTYLE);
			log_info("smtp-in: Failed command on session %016"PRIx64
			    ": \"%s\" => %.*s", s->id, tmp, n, buf);
		}
		break;
	}
}

static void
smtp_free(struct smtp_session *s, const char * reason)
{
	struct smtp_rcpt	*rcpt;

	log_debug("debug: smtp: %p: deleting session: %s", s, reason);

	tree_pop(&wait_mfa_data, s->id);

	if (s->ofile)
		fclose(s->ofile);

	if (s->evp.id) {
		m_create(p_queue, IMSG_SMTP_MESSAGE_ROLLBACK, 0, 0, -1);
		m_add_msgid(p_queue, evpid_to_msgid(s->evp.id));
		m_close(p_queue);
	}

	if (s->flags & SF_MFACONNSENT)
		smtp_filter_disconnect(s);

	if (s->flags & SF_SECURE && s->listener->flags & F_SMTPS)
		stat_decrement("smtp.smtps", 1);
	if (s->flags & SF_SECURE && s->listener->flags & F_STARTTLS)
		stat_decrement("smtp.tls", 1);

	while ((rcpt = TAILQ_FIRST(&s->rcpts))) {
		TAILQ_REMOVE(&s->rcpts, rcpt, entry);
		free(rcpt);
	}

	rfc2822_parser_release(&s->rfc2822_parser);

	io_clear(&s->io);
	iobuf_clear(&s->iobuf);
	free(s);

	smtp_collect();
}

static int
smtp_mailaddr(struct mailaddr *maddr, char *line, int mailfrom, char **args,
    const char *domain)
{
	char   *p, *e;

	if (line == NULL)
		return (0);

	if (*line != '<')
		return (0);

	e = strchr(line, '>');
	if (e == NULL)
		return (0);
	*e++ = '\0';
	while (*e == ' ')
		e++;
	*args = e;

	if (!text_to_mailaddr(maddr, line + 1))
		return (0);

	p = strchr(maddr->user, ':');
	if (p != NULL) {
		p++;
		memmove(maddr->user, p, strlen(p) + 1);
	}

	if (!valid_localpart(maddr->user) ||
	    !valid_domainpart(maddr->domain)) {
		/* accept empty return-path in MAIL FROM, required for bounces */
		if (mailfrom && maddr->user[0] == '\0' && maddr->domain[0] == '\0')
			return (1);

		/* no user-part, reject */
		if (maddr->user[0] == '\0')
			return (0);

		/* no domain, local user */
		if (maddr->domain[0] == '\0') {
			(void)strlcpy(maddr->domain, domain,
			    sizeof(maddr->domain));
			return (1);
		}
		return (0);
	}

	return (1);
}

static int
smtp_verify_certificate(struct smtp_session *s)
{
	struct ca_vrfy_req_msg	req_ca_vrfy;
	struct iovec		iov[2];
	X509		       *x;
	STACK_OF(X509)	       *xchain;
	int			i;
	const char	       *pkiname;

	x = SSL_get_peer_certificate(s->io.ssl);
	if (x == NULL)
		return 0;
	xchain = SSL_get_peer_cert_chain(s->io.ssl);

	/*
	 * Client provided a certificate and possibly a certificate chain.
	 * SMTP can't verify because it does not have the information that
	 * it needs, instead it will pass the certificate and chain to the
	 * lookup process and wait for a reply.
	 *
	 */

	tree_xset(&wait_ssl_verify, s->id, s);

	/* Send the client certificate */
	memset(&req_ca_vrfy, 0, sizeof req_ca_vrfy);
	if (s->listener->pki_name[0])
		pkiname = s->listener->pki_name;
	else
		pkiname = s->smtpname;

	if (strlcpy(req_ca_vrfy.pkiname, pkiname, sizeof req_ca_vrfy.pkiname)
	    >= sizeof req_ca_vrfy.pkiname)
		return 0;

	req_ca_vrfy.reqid = s->id;
	req_ca_vrfy.cert_len = i2d_X509(x, &req_ca_vrfy.cert);
	if (xchain)
		req_ca_vrfy.n_chain = sk_X509_num(xchain);
	iov[0].iov_base = &req_ca_vrfy;
	iov[0].iov_len = sizeof(req_ca_vrfy);
	iov[1].iov_base = req_ca_vrfy.cert;
	iov[1].iov_len = req_ca_vrfy.cert_len;
	m_composev(p_lka, IMSG_SMTP_SSL_VERIFY_CERT, 0, 0, -1,
	    iov, nitems(iov));
	free(req_ca_vrfy.cert);
	X509_free(x);

	if (xchain) {		
		/* Send the chain, one cert at a time */
		for (i = 0; i < sk_X509_num(xchain); ++i) {
			memset(&req_ca_vrfy, 0, sizeof req_ca_vrfy);
			req_ca_vrfy.reqid = s->id;
			x = sk_X509_value(xchain, i);
			req_ca_vrfy.cert_len = i2d_X509(x, &req_ca_vrfy.cert);
			iov[0].iov_base = &req_ca_vrfy;
			iov[0].iov_len  = sizeof(req_ca_vrfy);
			iov[1].iov_base = req_ca_vrfy.cert;
			iov[1].iov_len  = req_ca_vrfy.cert_len;
			m_composev(p_lka, IMSG_SMTP_SSL_VERIFY_CHAIN, 0, 0, -1,
			    iov, nitems(iov));
			free(req_ca_vrfy.cert);
		}
	}

	/* Tell lookup process that it can start verifying, we're done */
	memset(&req_ca_vrfy, 0, sizeof req_ca_vrfy);
	req_ca_vrfy.reqid = s->id;
	m_compose(p_lka, IMSG_SMTP_SSL_VERIFY, 0, 0, -1,
	    &req_ca_vrfy, sizeof req_ca_vrfy);

	return 1;
}

static void
smtp_auth_failure_resume(int fd, short event, void *p)
{
	struct smtp_session *s = p;

	smtp_reply(s, "535 Authentication failed");
	smtp_enter_state(s, STATE_HELO);
	io_reload(&s->io);
}

static void
smtp_auth_failure_pause(struct smtp_session *s)
{
	struct timeval	tv;

	tv.tv_sec = 0;
	tv.tv_usec = arc4random_uniform(1000000);
	log_trace(TRACE_SMTP, "smtp: timing-attack protection triggered, "
	    "will defer answer for %lu microseconds", tv.tv_usec);
	evtimer_set(&s->pause, smtp_auth_failure_resume, s);
	evtimer_add(&s->pause, &tv);
}

static int
smtp_sni_callback(SSL *ssl, int *ad, void *arg)
{
	const char		*sn;
	struct smtp_session	*s = arg;
	void			*ssl_ctx;

	sn = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	if (sn == NULL)
		return SSL_TLSEXT_ERR_NOACK;
	if (strlcpy(s->sni, sn, sizeof s->sni) >= sizeof s->sni) {
		log_warnx("warn: client SNI exceeds max hostname length");
		return SSL_TLSEXT_ERR_NOACK;
	}
	ssl_ctx = dict_get(env->sc_ssl_dict, sn);
	if (ssl_ctx == NULL) {
		log_info("smtp-in: No PKI entry for requested SNI \"%s\""
		    "on session %016"PRIx64, sn, s->id);
		return SSL_TLSEXT_ERR_NOACK;
	}
	SSL_set_SSL_CTX(ssl, ssl_ctx);
	return SSL_TLSEXT_ERR_OK;
}

static void
smtp_filter_rset(struct smtp_session *s)
{
}

static void
smtp_filter_commit(struct smtp_session *s)
{
}

static void
smtp_filter_rollback(struct smtp_session *s)
{
}

static void
smtp_filter_disconnect(struct smtp_session *s)
{
}

static void
smtp_filter_connect(struct smtp_session *s, struct sockaddr *sa)
{
	smtp_mfa_response(s, IMSG_SMTP_REQ_CONNECT, FILTER_OK, 0, NULL);
}

static void
smtp_filter_eom(struct smtp_session *s)
{
	smtp_mfa_response(s, IMSG_SMTP_REQ_EOM, FILTER_OK, 0, NULL);
}

static void
smtp_filter_helo(struct smtp_session *s)
{
	smtp_mfa_response(s, IMSG_SMTP_REQ_HELO, FILTER_OK, 0, NULL);
}

static void
smtp_filter_mail(struct smtp_session *s)
{
	smtp_mfa_response(s, IMSG_SMTP_REQ_MAIL, FILTER_OK, 0, NULL);
}

static void
smtp_filter_rcpt(struct smtp_session *s)
{
	smtp_mfa_response(s, IMSG_SMTP_REQ_RCPT, FILTER_OK, 0, NULL);
}

static void
smtp_filter_data(struct smtp_session *s)
{
	smtp_mfa_response(s, IMSG_SMTP_REQ_DATA, FILTER_OK, 0, NULL);
}

#define CASE(x) case x : return #x

const char *
smtp_strstate(int state)
{
	static char	buf[32];

	switch (state) {
	CASE(STATE_NEW);
	CASE(STATE_CONNECTED);
	CASE(STATE_TLS);
	CASE(STATE_HELO);
	CASE(STATE_AUTH_INIT);
	CASE(STATE_AUTH_USERNAME);
	CASE(STATE_AUTH_PASSWORD);
	CASE(STATE_AUTH_FINALIZE);
	CASE(STATE_BODY);
	CASE(STATE_QUIT);
	default:
		(void)snprintf(buf, sizeof(buf), "STATE_??? (%d)", state);
		return (buf);
	}
}
