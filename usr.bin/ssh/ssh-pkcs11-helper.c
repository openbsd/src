/* $OpenBSD: ssh-pkcs11-helper.c,v 1.19 2019/06/06 05:13:13 otto Exp $ */
/*
 * Copyright (c) 2010 Markus Friedl.  All rights reserved.
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
#include <sys/time.h>

#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "xmalloc.h"
#include "sshbuf.h"
#include "log.h"
#include "misc.h"
#include "sshkey.h"
#include "authfd.h"
#include "ssh-pkcs11.h"
#include "ssherr.h"

/* borrows code from sftp-server and ssh-agent */

struct pkcs11_keyinfo {
	struct sshkey	*key;
	char		*providername;
	TAILQ_ENTRY(pkcs11_keyinfo) next;
};

TAILQ_HEAD(, pkcs11_keyinfo) pkcs11_keylist;

#define MAX_MSG_LENGTH		10240 /*XXX*/

/* input and output queue */
struct sshbuf *iqueue;
struct sshbuf *oqueue;

static void
add_key(struct sshkey *k, char *name)
{
	struct pkcs11_keyinfo *ki;

	ki = xcalloc(1, sizeof(*ki));
	ki->providername = xstrdup(name);
	ki->key = k;
	TAILQ_INSERT_TAIL(&pkcs11_keylist, ki, next);
}

static void
del_keys_by_name(char *name)
{
	struct pkcs11_keyinfo *ki, *nxt;

	for (ki = TAILQ_FIRST(&pkcs11_keylist); ki; ki = nxt) {
		nxt = TAILQ_NEXT(ki, next);
		if (!strcmp(ki->providername, name)) {
			TAILQ_REMOVE(&pkcs11_keylist, ki, next);
			free(ki->providername);
			sshkey_free(ki->key);
			free(ki);
		}
	}
}

/* lookup matching 'private' key */
static struct sshkey *
lookup_key(struct sshkey *k)
{
	struct pkcs11_keyinfo *ki;

	TAILQ_FOREACH(ki, &pkcs11_keylist, next) {
		debug("check %p %s", ki, ki->providername);
		if (sshkey_equal(k, ki->key))
			return (ki->key);
	}
	return (NULL);
}

static void
send_msg(struct sshbuf *m)
{
	int r;

	if ((r = sshbuf_put_stringb(oqueue, m)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
}

static void
process_add(void)
{
	char *name, *pin;
	struct sshkey **keys = NULL;
	int r, i, nkeys;
	u_char *blob;
	size_t blen;
	struct sshbuf *msg;

	if ((msg = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	if ((r = sshbuf_get_cstring(iqueue, &name, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(iqueue, &pin, NULL)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	if ((nkeys = pkcs11_add_provider(name, pin, &keys)) > 0) {
		if ((r = sshbuf_put_u8(msg,
		    SSH2_AGENT_IDENTITIES_ANSWER)) != 0 ||
		    (r = sshbuf_put_u32(msg, nkeys)) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
		for (i = 0; i < nkeys; i++) {
			if ((r = sshkey_to_blob(keys[i], &blob, &blen)) != 0) {
				debug("%s: sshkey_to_blob: %s",
				    __func__, ssh_err(r));
				continue;
			}
			if ((r = sshbuf_put_string(msg, blob, blen)) != 0 ||
			    (r = sshbuf_put_cstring(msg, name)) != 0)
				fatal("%s: buffer error: %s",
				    __func__, ssh_err(r));
			free(blob);
			add_key(keys[i], name);
		}
	} else {
		if ((r = sshbuf_put_u8(msg, SSH_AGENT_FAILURE)) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
		if ((r = sshbuf_put_u32(msg, -nkeys)) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
	}
	free(keys);
	free(pin);
	free(name);
	send_msg(msg);
	sshbuf_free(msg);
}

static void
process_del(void)
{
	char *name, *pin;
	struct sshbuf *msg;
	int r;

	if ((msg = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	if ((r = sshbuf_get_cstring(iqueue, &name, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(iqueue, &pin, NULL)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	del_keys_by_name(name);
	if ((r = sshbuf_put_u8(msg, pkcs11_del_provider(name) == 0 ?
	    SSH_AGENT_SUCCESS : SSH_AGENT_FAILURE)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	free(pin);
	free(name);
	send_msg(msg);
	sshbuf_free(msg);
}

static void
process_sign(void)
{
	u_char *blob, *data, *signature = NULL;
	size_t blen, dlen, slen = 0;
	int r, ok = -1;
	struct sshkey *key, *found;
	struct sshbuf *msg;

	/* XXX support SHA2 signature flags */
	if ((r = sshbuf_get_string(iqueue, &blob, &blen)) != 0 ||
	    (r = sshbuf_get_string(iqueue, &data, &dlen)) != 0 ||
	    (r = sshbuf_get_u32(iqueue, NULL)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));

	if ((r = sshkey_from_blob(blob, blen, &key)) != 0)
		error("%s: sshkey_from_blob: %s", __func__, ssh_err(r));
	else {
		if ((found = lookup_key(key)) != NULL) {
#ifdef WITH_OPENSSL
			int ret;

			if (key->type == KEY_RSA) {
				slen = RSA_size(key->rsa);
				signature = xmalloc(slen);
				ret = RSA_private_encrypt(dlen, data, signature,
				    found->rsa, RSA_PKCS1_PADDING);
				if (ret != -1) {
					slen = ret;
					ok = 0;
				}
			} else if (key->type == KEY_ECDSA) {
				u_int xslen = ECDSA_size(key->ecdsa);

				signature = xmalloc(xslen);
				/* "The parameter type is ignored." */
				ret = ECDSA_sign(-1, data, dlen, signature,
				    &xslen, found->ecdsa);
				if (ret != 0)
					ok = 0;
				else
					error("%s: ECDSA_sign"
					    " returns %d", __func__, ret);
				slen = xslen;
			} else
				error("%s: don't know how to sign with key "
				    "type %d", __func__, (int)key->type);
#endif /* WITH_OPENSSL */
		}
		sshkey_free(key);
	}
	if ((msg = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	if (ok == 0) {
		if ((r = sshbuf_put_u8(msg, SSH2_AGENT_SIGN_RESPONSE)) != 0 ||
		    (r = sshbuf_put_string(msg, signature, slen)) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
	} else {
		if ((r = sshbuf_put_u8(msg, SSH2_AGENT_FAILURE)) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
	}
	free(data);
	free(blob);
	free(signature);
	send_msg(msg);
	sshbuf_free(msg);
}

static void
process(void)
{
	u_int msg_len;
	u_int buf_len;
	u_int consumed;
	u_char type;
	const u_char *cp;
	int r;

	buf_len = sshbuf_len(iqueue);
	if (buf_len < 5)
		return;		/* Incomplete message. */
	cp = sshbuf_ptr(iqueue);
	msg_len = get_u32(cp);
	if (msg_len > MAX_MSG_LENGTH) {
		error("bad message len %d", msg_len);
		cleanup_exit(11);
	}
	if (buf_len < msg_len + 4)
		return;
	if ((r = sshbuf_consume(iqueue, 4)) != 0 ||
	    (r = sshbuf_get_u8(iqueue, &type)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	buf_len -= 4;
	switch (type) {
	case SSH_AGENTC_ADD_SMARTCARD_KEY:
		debug("process_add");
		process_add();
		break;
	case SSH_AGENTC_REMOVE_SMARTCARD_KEY:
		debug("process_del");
		process_del();
		break;
	case SSH2_AGENTC_SIGN_REQUEST:
		debug("process_sign");
		process_sign();
		break;
	default:
		error("Unknown message %d", type);
		break;
	}
	/* discard the remaining bytes from the current packet */
	if (buf_len < sshbuf_len(iqueue)) {
		error("iqueue grew unexpectedly");
		cleanup_exit(255);
	}
	consumed = buf_len - sshbuf_len(iqueue);
	if (msg_len < consumed) {
		error("msg_len %d < consumed %d", msg_len, consumed);
		cleanup_exit(255);
	}
	if (msg_len > consumed) {
		if ((r = sshbuf_consume(iqueue, msg_len - consumed)) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
	}
}

void
cleanup_exit(int i)
{
	/* XXX */
	_exit(i);
}


int
main(int argc, char **argv)
{
	int r, ch, in, out, max, log_stderr = 0;
	ssize_t len;
	SyslogFacility log_facility = SYSLOG_FACILITY_AUTH;
	LogLevel log_level = SYSLOG_LEVEL_ERROR;
	char buf[4*4096];
	extern char *__progname;
	struct pollfd pfd[2];

	TAILQ_INIT(&pkcs11_keylist);

	log_init(__progname, log_level, log_facility, log_stderr);

	while ((ch = getopt(argc, argv, "v")) != -1) {
		switch (ch) {
		case 'v':
			log_stderr = 1;
			if (log_level == SYSLOG_LEVEL_ERROR)
				log_level = SYSLOG_LEVEL_DEBUG1;
			else if (log_level < SYSLOG_LEVEL_DEBUG3)
				log_level++;
			break;
		default:
			fprintf(stderr, "usage: %s [-v]\n", __progname);
			exit(1);
		}
	}

	log_init(__progname, log_level, log_facility, log_stderr);

	pkcs11_init(0);
	in = STDIN_FILENO;
	out = STDOUT_FILENO;

	max = 0;
	if (in > max)
		max = in;
	if (out > max)
		max = out;

	if ((iqueue = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	if ((oqueue = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);

	while (1) {
		memset(pfd, 0, sizeof(pfd));
		pfd[0].fd = in;
		pfd[1].fd = out;

		/*
		 * Ensure that we can read a full buffer and handle
		 * the worst-case length packet it can generate,
		 * otherwise apply backpressure by stopping reads.
		 */
		if ((r = sshbuf_check_reserve(iqueue, sizeof(buf))) == 0 &&
		    (r = sshbuf_check_reserve(oqueue, MAX_MSG_LENGTH)) == 0)
			pfd[0].events = POLLIN;
		else if (r != SSH_ERR_NO_BUFFER_SPACE)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));

		if (sshbuf_len(oqueue) > 0)
			pfd[1].events = POLLOUT;

		if ((r = poll(pfd, 2, -1 /* INFTIM */)) <= 0) {
			if (r == 0 || errno == EINTR)
				continue;
			fatal("poll: %s", strerror(errno));
		}

		/* copy stdin to iqueue */
		if ((pfd[0].revents & (POLLIN|POLLERR)) != 0) {
			len = read(in, buf, sizeof buf);
			if (len == 0) {
				debug("read eof");
				cleanup_exit(0);
			} else if (len < 0) {
				error("read: %s", strerror(errno));
				cleanup_exit(1);
			} else if ((r = sshbuf_put(iqueue, buf, len)) != 0) {
				fatal("%s: buffer error: %s",
				    __func__, ssh_err(r));
			}
		}
		/* send oqueue to stdout */
		if ((pfd[1].revents & (POLLOUT|POLLHUP)) != 0) {
			len = write(out, sshbuf_ptr(oqueue),
			    sshbuf_len(oqueue));
			if (len < 0) {
				error("write: %s", strerror(errno));
				cleanup_exit(1);
			} else if ((r = sshbuf_consume(oqueue, len)) != 0) {
				fatal("%s: buffer error: %s",
				    __func__, ssh_err(r));
			}
		}

		/*
		 * Process requests from client if we can fit the results
		 * into the output buffer, otherwise stop processing input
		 * and let the output queue drain.
		 */
		if ((r = sshbuf_check_reserve(oqueue, MAX_MSG_LENGTH)) == 0)
			process();
		else if (r != SSH_ERR_NO_BUFFER_SPACE)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
	}
}
