/*	$OpenBSD: ssl.c,v 1.50 2012/11/12 14:58:53 eric Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <ctype.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/engine.h>
#include <openssl/err.h>

#include "smtpd.h"
#include "log.h"

#define SSL_CIPHERS	"HIGH"

void	 ssl_error(const char *);
char	*ssl_load_file(const char *, off_t *, mode_t);
SSL_CTX	*ssl_ctx_create(void);

SSL	*ssl_client_init(int, char *, size_t, char *, size_t);

DH	*get_dh1024(void);
DH	*get_dh_from_memory(char *, size_t);
void	 ssl_set_ephemeral_key_exchange(SSL_CTX *, DH *);

extern int ssl_ctx_load_verify_memory(SSL_CTX *, char *, off_t);

int
ssl_cmp(struct ssl *s1, struct ssl *s2)
{
	return (strcmp(s1->ssl_name, s2->ssl_name));
}

SPLAY_GENERATE(ssltree, ssl, ssl_nodes, ssl_cmp);

char *
ssl_load_file(const char *name, off_t *len, mode_t perm)
{
	struct stat	 st;
	off_t		 size;
	char		*buf = NULL;
	int		 fd, saved_errno;
	char		 mode[12];

	if ((fd = open(name, O_RDONLY)) == -1)
		return (NULL);
	if (fstat(fd, &st) != 0)
		goto fail;
	if (st.st_uid != 0) {
		log_warnx("warn:  %s: not owned by uid 0", name);
		errno = EACCES;
		goto fail;
	}
	if (st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) & ~perm) {
		strmode(perm, mode);
		log_warnx("warn:  %s: insecure permissions: must be at most %s",
		    name, &mode[1]);
		errno = EACCES;
		goto fail;
	}
	size = st.st_size;
	if ((buf = calloc(1, size + 1)) == NULL)
		goto fail;
	if (read(fd, buf, size) != size)
		goto fail;
	close(fd);

	*len = size + 1;
	return (buf);

fail:
	if (buf != NULL)
		free(buf);
	saved_errno = errno;
	close(fd);
	errno = saved_errno;
	return (NULL);
}

SSL_CTX *
ssl_ctx_create(void)
{
	SSL_CTX	*ctx;

	ctx = SSL_CTX_new(SSLv23_method());
	if (ctx == NULL) {
		ssl_error("ssl_ctx_create");
		fatal("ssl_ctx_create: could not create SSL context");
	}

	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
	SSL_CTX_set_timeout(ctx, SMTPD_SESSION_TIMEOUT);
	SSL_CTX_set_options(ctx,
	    SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_TICKET);
	SSL_CTX_set_options(ctx,
	    SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

	if (!SSL_CTX_set_cipher_list(ctx, SSL_CIPHERS)) {
		ssl_error("ssl_ctx_create");
		fatal("ssl_ctx_create: could not set cipher list");
	}

	return (ctx);
}

int
ssl_load_certfile(const char *name, uint8_t flags)
{
	struct ssl	*s;
	struct ssl	 key;
	char		 certfile[PATH_MAX];

	if (strlcpy(key.ssl_name, name, sizeof(key.ssl_name))
	    >= sizeof(key.ssl_name)) {
		log_warnx("warn:  ssl_load_certfile: certificate name truncated");
		return -1;
	}

	s = SPLAY_FIND(ssltree, env->sc_ssl, &key);
	if (s != NULL) {
		s->flags |= flags;
		return 0;
	}

	if ((s = calloc(1, sizeof(*s))) == NULL)
		fatal(NULL);

	s->flags = flags;
	(void)strlcpy(s->ssl_name, key.ssl_name, sizeof(s->ssl_name));

	if (! bsnprintf(certfile, sizeof(certfile),
		"/etc/mail/certs/%s.crt", name))
		goto err;

	s->ssl_cert = ssl_load_file(certfile, &s->ssl_cert_len, 0755);
	if (s->ssl_cert == NULL)
		goto err;

	if (! bsnprintf(certfile, sizeof(certfile),
		"/etc/mail/certs/%s.key", name))
		goto err;

	s->ssl_key = ssl_load_file(certfile, &s->ssl_key_len, 0700);
	if (s->ssl_key == NULL)
		goto err;

	if (! bsnprintf(certfile, sizeof(certfile),
		"/etc/mail/certs/%s.ca", name))
		goto err;

	s->ssl_ca = ssl_load_file(certfile, &s->ssl_ca_len, 0755);
	if (s->ssl_ca == NULL) {
		if (errno == EACCES)
			goto err;
		log_warnx("warn:  no CA found in %s", certfile);
	}

	if (! bsnprintf(certfile, sizeof(certfile),
		"/etc/mail/certs/%s.dh", name))
		goto err;

	s->ssl_dhparams = ssl_load_file(certfile, &s->ssl_dhparams_len, 0755);
	if (s->ssl_dhparams == NULL) {
		if (errno == EACCES)
			goto err;
		log_info("info: No DH parameters found in %s: "
		    "using built-in parameters", certfile);
	}

	SPLAY_INSERT(ssltree, env->sc_ssl, s);

	return (0);
err:
	if (s->ssl_cert != NULL)
		free(s->ssl_cert);
	if (s->ssl_key != NULL)
		free(s->ssl_key);
	if (s->ssl_dhparams != NULL)
		free(s->ssl_dhparams);
	if (s != NULL)
		free(s);
	return (-1);
}

void
ssl_init(void)
{
	SSL_library_init();
	SSL_load_error_strings();

	OpenSSL_add_all_algorithms();

	/* Init hardware crypto engines. */
	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();
}

void
ssl_setup(struct listener *l)
{
	struct ssl	key;
	DH *dh;

	if (!(l->flags & F_SSL))
		return;

	if (strlcpy(key.ssl_name, l->ssl_cert_name, sizeof(key.ssl_name))
	    >= sizeof(key.ssl_name))
		fatal("ssl_setup: certificate name truncated");

	if ((l->ssl = SPLAY_FIND(ssltree, env->sc_ssl, &key)) == NULL)
		fatal("ssl_setup: certificate tree corrupted");

	l->ssl_ctx = ssl_ctx_create();

	if (l->ssl->ssl_ca != NULL) {
		if (! ssl_ctx_load_verify_memory(l->ssl_ctx,
			l->ssl->ssl_ca, l->ssl->ssl_ca_len))
			goto err;
	}

	if (!ssl_ctx_use_certificate_chain(l->ssl_ctx,
	    l->ssl->ssl_cert, l->ssl->ssl_cert_len))
		goto err;
	if (!ssl_ctx_use_private_key(l->ssl_ctx,
	    l->ssl->ssl_key, l->ssl->ssl_key_len))
		goto err;

	if (!SSL_CTX_check_private_key(l->ssl_ctx))
		goto err;
	if (!SSL_CTX_set_session_id_context(l->ssl_ctx,
		(const unsigned char *)l->ssl_cert_name,
		strlen(l->ssl_cert_name) + 1))
		goto err;

	if (l->ssl->ssl_dhparams_len == 0)
		dh = get_dh1024();
	else
		dh = get_dh_from_memory(l->ssl->ssl_dhparams,
		    l->ssl->ssl_dhparams_len);
	ssl_set_ephemeral_key_exchange(l->ssl_ctx, dh);
	DH_free(dh);

	log_debug("debug: ssl_setup: ssl setup finished for listener: %p", l);
	return;

err:
	if (l->ssl_ctx != NULL)
		SSL_CTX_free(l->ssl_ctx);
	ssl_error("ssl_setup");
	fatal("ssl_setup: cannot set SSL up");
	return;
}

const char *
ssl_to_text(void *ssl) {
	static char buf[256];

	snprintf(buf, sizeof buf, "version=%s, cipher=%s, bits=%i",
	    SSL_get_cipher_version(ssl),
	    SSL_get_cipher_name(ssl),
	    SSL_get_cipher_bits(ssl, NULL));

	return (buf);
}

void
ssl_error(const char *where)
{
	unsigned long	code;
	char		errbuf[128];
	extern int	debug;

	if (!debug)
		return;
	for (; (code = ERR_get_error()) != 0 ;) {
		ERR_error_string_n(code, errbuf, sizeof(errbuf));
		log_debug("debug: SSL library error: %s: %s", where, errbuf);
	}
}

SSL *
ssl_client_init(int fd, char *cert, size_t certsz, char *key, size_t keysz)
{
	SSL_CTX		*ctx;
	SSL		*ssl = NULL;
	int		 rv = -1;

	ctx = ssl_ctx_create();

	if (cert && key) {
		if (!ssl_ctx_use_certificate_chain(ctx, cert, certsz))
			goto done;
		else if (!ssl_ctx_use_private_key(ctx, key, keysz))
			goto done;
		else if (!SSL_CTX_check_private_key(ctx))
			goto done;
	}

	if ((ssl = SSL_new(ctx)) == NULL)
		goto done;
	SSL_CTX_free(ctx);

	if (!SSL_set_ssl_method(ssl, SSLv23_client_method()))
		goto done;
	if (!SSL_set_fd(ssl, fd))
		goto done;
	SSL_set_connect_state(ssl);

	rv = 0;
done:
	if (rv) {
		if (ssl)
			SSL_free(ssl);
		else if (ctx)
			SSL_CTX_free(ctx);
		ssl = NULL;
	}
	return (ssl);
}

void *
ssl_mta_init(struct ssl *s)
{
	SSL_CTX		*ctx;
	SSL		*ssl = NULL;
	int		 rv = -1;

	ctx = ssl_ctx_create();

	if (s && s->ssl_cert && s->ssl_key) {
		if (!ssl_ctx_use_certificate_chain(ctx,
		    s->ssl_cert, s->ssl_cert_len))
			goto done;
		else if (!ssl_ctx_use_private_key(ctx,
		    s->ssl_key, s->ssl_key_len))
			goto done;
		else if (!SSL_CTX_check_private_key(ctx))
			goto done;
	}

	if ((ssl = SSL_new(ctx)) == NULL)
		goto done;
	SSL_CTX_free(ctx);

	if (!SSL_set_ssl_method(ssl, SSLv23_client_method()))
		goto done;

	rv = 0;
done:
	if (rv) {
		if (ssl)
			SSL_free(ssl);
		else if (ctx)
			SSL_CTX_free(ctx);
		ssl = NULL;
	}
	return (void*)(ssl);
}

void *
ssl_smtp_init(void *ssl_ctx)
{
	SSL *ssl;

	log_debug("debug: session_start_ssl: switching to SSL");

	if ((ssl = SSL_new(ssl_ctx)) == NULL)
                goto err;
        if (!SSL_set_ssl_method(ssl, SSLv23_server_method()))
                goto err;

        return (void*)(ssl);

    err:
	if (ssl != NULL)
		SSL_free(ssl);
	ssl_error("ssl_session_init");
	return (NULL);
}


/* From OpenSSL's documentation:
 *
 * If "strong" primes were used to generate the DH parameters, it is
 * not strictly necessary to generate a new key for each handshake
 * but it does improve forward secrecy.
 *
 * -- gilles@
 */
DH *
get_dh1024(void)
{
	DH *dh;
	unsigned char dh1024_p[] = {
		0xAD,0x37,0xBB,0x26,0x75,0x01,0x27,0x75,
		0x06,0xB5,0xE7,0x1E,0x1F,0x2B,0xBC,0x51,
		0xC0,0xF4,0xEB,0x42,0x7A,0x2A,0x83,0x1E,
		0xE8,0xD1,0xD8,0xCC,0x9E,0xE6,0x15,0x1D,
		0x06,0x46,0x50,0x94,0xB9,0xEE,0xB6,0x89,
		0xB7,0x3C,0xAC,0x07,0x5E,0x29,0x37,0xCC,
		0x8F,0xDF,0x48,0x56,0x85,0x83,0x26,0x02,
		0xB8,0xB6,0x63,0xAF,0x2D,0x4A,0x57,0x93,
		0x6B,0x54,0xE1,0x8F,0x28,0x76,0x9C,0x5D,
		0x90,0x65,0xD1,0x07,0xFE,0x5B,0x05,0x65,
		0xDA,0xD2,0xE2,0xAF,0x23,0xCA,0x2F,0xD6,
		0x4B,0xD2,0x04,0xFE,0xDF,0x21,0x2A,0xE1,
		0xCD,0x1B,0x70,0x76,0xB3,0x51,0xA4,0xC9,
		0x2B,0x68,0xE3,0xDD,0xCB,0x97,0xDA,0x59,
		0x50,0x93,0xEE,0xDB,0xBF,0xC7,0xFA,0xA7,
		0x47,0xC4,0x4D,0xF0,0xC6,0x09,0x4A,0x4B
	};
	unsigned char dh1024_g[] = {
		0x02
	};

        if ((dh = DH_new()) == NULL)
		return NULL;

        dh->p = BN_bin2bn(dh1024_p, sizeof(dh1024_p), NULL);
        dh->g = BN_bin2bn(dh1024_g, sizeof(dh1024_g), NULL);
        if (dh->p == NULL || dh->g == NULL) {
		DH_free(dh);
		return NULL;
	}

        return dh;
}

DH *
get_dh_from_memory(char *params, size_t len)
{
	BIO *mem;
        DH *dh;

	mem = BIO_new_mem_buf(params, len);
	if (mem == NULL)
		return NULL;
	dh = PEM_read_bio_DHparams(mem, NULL, NULL, NULL);
	if (dh == NULL)
		goto err;
        if (dh->p == NULL || dh->g == NULL)
		goto err;
	return dh;

err:
	if (mem != NULL)
		BIO_free(mem);
	if (dh != NULL)
		DH_free(dh);
	return NULL;
}


void
ssl_set_ephemeral_key_exchange(SSL_CTX *ctx, DH *dh)
{
	if (dh == NULL || !SSL_CTX_set_tmp_dh(ctx, dh)) {
		ssl_error("ssl_set_ephemeral_key_exchange");
		fatal("ssl_set_ephemeral_key_exchange: cannot set tmp dh");
	}
}
