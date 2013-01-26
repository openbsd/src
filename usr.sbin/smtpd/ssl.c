/*	$OpenBSD: ssl.c,v 1.52 2013/01/26 09:37:24 gilles Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2012 Gilles Chehade <gilles@poolp.org>
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

#include "log.h"
#include "ssl.h"

void
ssl_init(void)
{
	static int	inited = 0;

	if (inited)
		return;

	SSL_library_init();
	SSL_load_error_strings();
	
	OpenSSL_add_all_algorithms();
	
	/* Init hardware crypto engines. */
	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();
	inited = 1;
}

int
ssl_setup(SSL_CTX **ctxp, struct ssl *ssl)
{
	DH	*dh;
	SSL_CTX	*ctx;
	
	ctx = ssl_ctx_create();

	if (!ssl_ctx_use_certificate_chain(ctx,
		ssl->ssl_cert, ssl->ssl_cert_len))
		goto err;
	if (!ssl_ctx_use_private_key(ctx,
		ssl->ssl_key, ssl->ssl_key_len))
		goto err;

	if (!SSL_CTX_check_private_key(ctx))
		goto err;
	if (!SSL_CTX_set_session_id_context(ctx,
		(const unsigned char *)ssl->ssl_name,
		strlen(ssl->ssl_name) + 1))
		goto err;

	if (ssl->ssl_dhparams_len == 0)
		dh = get_dh1024();
	else
		dh = get_dh_from_memory(ssl->ssl_dhparams,
		    ssl->ssl_dhparams_len);
	ssl_set_ephemeral_key_exchange(ctx, dh);
	DH_free(dh);

	*ctxp = ctx;
	return 1;

err:
	SSL_CTX_free(ctx);
	ssl_error("ssl_setup");
	return 0;
}

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

static int
ssl_password_cb(char *buf, int size, int rwflag, void *u)
{
	size_t	len;
	if (u == NULL) {
		bzero(buf, size);
		return (0);
	}
	if ((len = strlcpy(buf, u, size)) >= (size_t)size)
		return (0);
	return (len);
}

char *
ssl_load_key(const char *name, off_t *len, char *pass)
{
	FILE		*fp;
	EVP_PKEY	*key = NULL;
	BIO		*bio = NULL;
	long		 size;
	char		*data, *buf = NULL;

	/* Initialize SSL library once */
	ssl_init();

	/*
	 * Read (possibly) encrypted key from file
	 */
	if ((fp = fopen(name, "r")) == NULL)
		return (NULL);

	key = PEM_read_PrivateKey(fp, NULL, ssl_password_cb, pass);
	fclose(fp);
	if (key == NULL)
		goto fail;

	/*
	 * Write unencrypted key to memory buffer
	 */
	if ((bio = BIO_new(BIO_s_mem())) == NULL)
		goto fail;
	if (!PEM_write_bio_PrivateKey(bio, key, NULL, NULL, 0, NULL, NULL))
		goto fail;
	if ((size = BIO_get_mem_data(bio, &data)) <= 0)
		goto fail;
	if ((buf = calloc(1, size)) == NULL)
		goto fail;
	memcpy(buf, data, size);

	BIO_free_all(bio);
	*len = (off_t)size;
	return (buf);

fail:
	ssl_error("ssl_load_key");

	free(buf);
	if (bio != NULL)
		BIO_free_all(bio);
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
	SSL_CTX_set_timeout(ctx, SSL_SESSION_TIMEOUT);
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
ssl_load_certfile(struct ssl **sp, const char *path, const char *name, uint8_t flags)
{
	struct ssl     *s;
	char		pathname[PATH_MAX];
	int		ret;

	if ((s = calloc(1, sizeof(*s))) == NULL)
		fatal(NULL);

	s->flags = flags;
	(void)strlcpy(s->ssl_name, name, sizeof(s->ssl_name));

	ret =  snprintf(pathname, sizeof(pathname), "%s/%s.crt",
	    path ? path : "/etc/ssl", name);
	if (ret == -1 || (size_t)ret >= sizeof pathname)
		goto err;
	s->ssl_cert = ssl_load_file(pathname, &s->ssl_cert_len, 0755);
	if (s->ssl_cert == NULL)
		goto err;

	ret = snprintf(pathname, sizeof(pathname), "%s/%s.key",
	    path ? path : "/etc/ssl/private", name);
	if (ret == -1 || (size_t)ret >= sizeof pathname)
		goto err;
	s->ssl_key = ssl_load_file(pathname, &s->ssl_key_len, 0700);
	if (s->ssl_key == NULL)
		goto err;

	ret = snprintf(pathname, sizeof(pathname), "%s/%s.ca",
	    path ? path : "/etc/ssl", name);
	if (ret == -1 || (size_t)ret >= sizeof pathname)
		goto err;
	s->ssl_ca = ssl_load_file(pathname, &s->ssl_ca_len, 0755);
	if (s->ssl_ca == NULL) {
		if (errno == EACCES)
			goto err;
		log_info("info: No CA found in %s", pathname);
	}

	ret = snprintf(pathname, sizeof(pathname), "%s/%s.dh",
	    path ? path : "/etc/ssl", name);
	if (ret == -1 || (size_t)ret >= sizeof pathname)
		goto err;
	s->ssl_dhparams = ssl_load_file(pathname, &s->ssl_dhparams_len, 0755);
	if (s->ssl_dhparams == NULL) {
		if (errno == EACCES)
			goto err;
		log_info("info: No DH parameters found in %s: "
		    "using built-in parameters", pathname);
	}

	*sp = s;
	return (1);

err:
	if (s->ssl_cert != NULL)
		free(s->ssl_cert);
	if (s->ssl_key != NULL)
		free(s->ssl_key);
	if (s->ssl_ca != NULL)
		free(s->ssl_ca);
	if (s->ssl_dhparams != NULL)
		free(s->ssl_dhparams);
	if (s != NULL)
		free(s);
	return (0);
}


const char *
ssl_to_text(const SSL *ssl)
{
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

	for (; (code = ERR_get_error()) != 0 ;) {
		if (!debug)
			continue;
		ERR_error_string_n(code, errbuf, sizeof(errbuf));
		log_debug("debug: SSL library error: %s: %s", where, errbuf);
	}
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
