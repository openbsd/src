/*	$OpenBSD: net_ssl.c,v 1.1 2005/03/30 18:44:49 ho Exp $	*/

/*
 * Copyright (c) 2005 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 * This code was written under funding by Multicom Security AB.
 */


#include <sys/types.h>

#include <openssl/ssl.h>
#include <openssl/tls1.h>
#include <openssl/err.h>

#include <string.h>

#include "sasyncd.h"
#include "net.h"

/* Global SSL context. */
SSL_CTX	*ctx;

static void	net_SSL_dump_stack(int);
static void	net_SSL_print_error(int, int);

int
net_SSL_init(void)
{
	SSL_library_init();
	SSL_load_error_strings();

	ctx = SSL_CTX_new(TLSv1_method());
	if (!ctx)
		return -1;

	(void)SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE | SSL_OP_NO_SSLv2);

	/* Load CA cert. */
	if (!SSL_CTX_load_verify_locations(ctx, cfgstate.cafile, NULL)) {
		net_SSL_dump_stack(0);
		fprintf(stderr, "cannot read \"%s\": %s\n", cfgstate.cafile,
		    strerror(errno));
		return -1;
	}

	/* Load our certificate. */
	if (!SSL_CTX_use_certificate_chain_file(ctx, cfgstate.certfile)) {
		net_SSL_dump_stack(0);
		fprintf(stderr, "cannot read \"%s\": %s\n", cfgstate.certfile,
		    strerror(errno));
		return -1;
	}

	/* Load and check private key. */
	if (!SSL_CTX_use_PrivateKey_file(ctx, cfgstate.privkeyfile,
	    SSL_FILETYPE_PEM)) {
		net_SSL_dump_stack(0);
		if (ERR_GET_REASON(ERR_peek_error() == EVP_R_BAD_DECRYPT)) {
			fprintf(stderr, "bad pass phrase\n");
			return -1;
		} else {
			fprintf(stderr, "cannot read \"%s\": %s\n",
			    cfgstate.privkeyfile, strerror(errno));
			return -1;
		}
	}
	if (!SSL_CTX_check_private_key(ctx)) {
		net_SSL_dump_stack(0);
		fprintf(stderr, "Private key does not match certificate\n");
		return -1;
	}
	return 0;
}

int
net_SSL_connect(struct syncpeer *p)
{
	int	r, err;

	p->ssl = SSL_new(ctx);
	if (!p->ssl)
		return -1;
	SSL_set_fd(p->ssl, p->socket);
	r = SSL_connect(p->ssl);
	if (r != 1) {
		err = SSL_get_error(p->ssl, r);
		net_SSL_print_error(err, r);
		return -1;
	}
	log_msg(2, "TLS connection established with peer "
	    "\"%s\"", p->name);
	return 0;
}

void
net_SSL_disconnect(struct syncpeer *p)
{
	if (p->ssl) {
		SSL_shutdown(p->ssl);
		SSL_free(p->ssl);
	}
	p->ssl = NULL;
}

static void
net_SSL_dump_stack(int level)
{
	int	err;

	while ((err = ERR_get_error()) != 0)
		log_msg(level, "%s", ERR_error_string(err, NULL));
}

static void
net_SSL_print_error(int r, int prev)
{
	char	*msg;

	switch (r) {
	case SSL_ERROR_NONE:
		msg = "SSL_ERROR_NONE";
		break;
	case SSL_ERROR_ZERO_RETURN:
		msg = "SSL_ERROR_ZERO_RETURN";
		break;
	case SSL_ERROR_WANT_READ:
		msg = "SSL_ERROR_WANT_READ";
		break;
	case SSL_ERROR_WANT_WRITE:
		msg = "SSL_ERROR_WANT_WRITE";
		break;
	case SSL_ERROR_WANT_CONNECT:
		msg = "SSL_ERROR_WANT_CONNECT";
		break;
	case SSL_ERROR_WANT_ACCEPT:
		msg = "SSL_ERROR_WANT_ACCEPT";
		break;
	case SSL_ERROR_WANT_X509_LOOKUP:
		msg = "SSL_ERROR_WANT_X509_LOOKUP";
		break;
	case SSL_ERROR_SYSCALL:
		msg = "SSL_ERROR_SYSCALL";
		break;
	case SSL_ERROR_SSL:
		msg = "SSL_ERROR_SSL";
		break;
	default:
		msg = "<unknown error>";
		break;
	}

	log_msg(3, "SSL: \"%s\" original code = %d", msg, prev);

	net_SSL_dump_stack(3);
}

static int
net_SSL_io(struct syncpeer *p, void *buf, u_int32_t len, int writeflag)
{
	int	ret, e;

  retry:
	if (writeflag)
		ret = SSL_write(p->ssl, buf, len);
	else
		ret = SSL_read(p->ssl, buf, len);
	if (ret == (int)len)
		return 0;

	e = SSL_get_error(p->ssl, ret);
	net_SSL_print_error(e, ret);

	if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE)
		goto retry; /* Enough to just retry here? XXX */

	log_msg(1, "peer \"%s\" disconnected", p->name);
	net_disconnect_peer(p);
	return 1;
}

int
net_SSL_read(struct syncpeer *p, void *buf, u_int32_t len)
{
	int r, err;

	if (!p->ssl) {
		p->ssl = SSL_new(ctx);
		if (!p->ssl) {
			log_msg(0, "SSL_new() failed");
			return NULL;
		}
		SSL_set_fd(p->ssl, p->socket);
		r = SSL_accept(p->ssl);
		if (r != 1) {
			err = SSL_get_error(p->ssl, r);
			net_SSL_print_error(err, r);
			return NULL;
		}
	}

	return net_SSL_io(p, buf, len, 0);
}

int
net_SSL_write(struct syncpeer *p, void *buf, u_int32_t len)
{
	return net_SSL_io(p, buf, len, 1);
}

void
net_SSL_shutdown(void)
{
	ERR_free_strings();
	SSL_CTX_free(ctx);
}
