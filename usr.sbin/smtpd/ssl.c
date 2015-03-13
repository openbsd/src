/*	$OpenBSD: ssl.c,v 1.76 2015/03/13 13:42:54 giovanni Exp $	*/

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
#include <sys/socket.h>
#include <sys/stat.h>

#include <ctype.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/dh.h>
#include <openssl/bn.h>

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
ssl_setup(SSL_CTX **ctxp, struct pki *pki)
{
	DH	*dh;
	SSL_CTX	*ctx;
	u_int8_t sid[SSL_MAX_SID_CTX_LENGTH];

	ctx = ssl_ctx_create(pki->pki_name, pki->pki_cert, pki->pki_cert_len);

	/*
	 * Set session ID context to a random value.  We don't support
	 * persistent caching of sessions so it is OK to set a temporary
	 * session ID context that is valid during run time.
	 */
	arc4random_buf(sid, sizeof(sid));
	if (!SSL_CTX_set_session_id_context(ctx, sid, sizeof(sid)))
		goto err;

	if (pki->pki_dhparams_len == 0)
		dh = get_dh1024();
	else
		dh = get_dh_from_memory(pki->pki_dhparams,
		    pki->pki_dhparams_len);
	ssl_set_ephemeral_key_exchange(ctx, dh);
	DH_free(dh);

	ssl_set_ecdh_curve(ctx, SSL_ECDH_CURVE);

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

#if 0
static int
ssl_password_cb(char *buf, int size, int rwflag, void *u)
{
	size_t	len;
	if (u == NULL) {
		explicit_bzero(buf, size);
		return (0);
	}
	if ((len = strlcpy(buf, u, size)) >= (size_t)size)
		return (0);
	return (len);
}
#endif

static int
ssl_password_cb(char *buf, int size, int rwflag, void *u)
{
	int	ret = 0;
	size_t	len;
	char	*pass;

	pass = getpass((const char *)u);
	if (pass == NULL)
		return 0;
	len = strlen(pass);
	if (strlcpy(buf, pass, size) >= (size_t)size)
		goto end;
	ret = len;
end:
	if (len)
		explicit_bzero(pass, len);
	return ret;
}

char *
ssl_load_key(const char *name, off_t *len, char *pass, mode_t perm, const char *pkiname)
{
	FILE		*fp = NULL;
	EVP_PKEY	*key = NULL;
	BIO		*bio = NULL;
	long		 size;
	char		*data, *buf = NULL;
	struct stat	 st;
	char		 mode[12];
	char		 prompt[2048];

	/* Initialize SSL library once */
	ssl_init();

	/*
	 * Read (possibly) encrypted key from file
	 */
	if ((fp = fopen(name, "r")) == NULL)
		return (NULL);

	if (fstat(fileno(fp), &st) != 0)
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

	(void)snprintf(prompt, sizeof prompt, "passphrase for %s: ", pkiname);
	key = PEM_read_PrivateKey(fp, NULL, ssl_password_cb, prompt);
	fclose(fp);
	fp = NULL;
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
	if ((buf = calloc(1, size + 1)) == NULL)
		goto fail;
	memcpy(buf, data, size);

	BIO_free_all(bio);
	EVP_PKEY_free(key);

	*len = (off_t)size + 1;
	return (buf);

fail:
	ssl_error("ssl_load_key");
	free(buf);
	if (bio != NULL)
		BIO_free_all(bio);
	if (key != NULL)
		EVP_PKEY_free(key);
	if (fp)
		fclose(fp);
	return (NULL);
}

SSL_CTX *
ssl_ctx_create(const char *pkiname, char *cert, off_t cert_len)
{
	SSL_CTX	*ctx;
	size_t	 pkinamelen = 0;

	ctx = SSL_CTX_new(SSLv23_method());
	if (ctx == NULL) {
		ssl_error("ssl_ctx_create");
		fatal("ssl_ctx_create: could not create SSL context");
	}

	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
	SSL_CTX_set_timeout(ctx, SSL_SESSION_TIMEOUT);
	SSL_CTX_set_options(ctx,
	    SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TICKET);
	SSL_CTX_set_options(ctx,
	    SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

	if (!SSL_CTX_set_cipher_list(ctx, SSL_CIPHERS)) {
		ssl_error("ssl_ctx_create");
		fatal("ssl_ctx_create: could not set cipher list");
	}

	if (cert != NULL) {
		if (pkiname != NULL)
			pkinamelen = strlen(pkiname) + 1;
		if (!SSL_CTX_use_certificate_chain_mem(ctx, cert, cert_len)) {
			ssl_error("ssl_ctx_create");
			fatal("ssl_ctx_create: invalid certificate chain");
		} else if (!ssl_ctx_fake_private_key(ctx,
		    pkiname, pkinamelen, cert, cert_len, NULL, NULL)) {
			ssl_error("ssl_ctx_create");
			fatal("ssl_ctx_create: could not fake private key");
		} else if (!SSL_CTX_check_private_key(ctx)) {
			ssl_error("ssl_ctx_create");
			fatal("ssl_ctx_create: invalid private key");
		}
	}

	return (ctx);
}

int
ssl_load_certificate(struct pki *p, const char *pathname)
{
	p->pki_cert = ssl_load_file(pathname, &p->pki_cert_len, 0755);
	if (p->pki_cert == NULL)
		return 0;
	return 1;
}

int
ssl_load_keyfile(struct pki *p, const char *pathname, const char *pkiname)
{
	char	pass[1024];

	p->pki_key = ssl_load_key(pathname, &p->pki_key_len, pass, 0700, pkiname);
	if (p->pki_key == NULL)
		return 0;
	return 1;
}

int
ssl_load_cafile(struct pki *p, const char *pathname)
{
	p->pki_ca = ssl_load_file(pathname, &p->pki_ca_len, 0755);
	if (p->pki_ca == NULL)
		return 0;
	return 1;
}

int
ssl_load_dhparams(struct pki *p, const char *pathname)
{
	p->pki_dhparams = ssl_load_file(pathname, &p->pki_dhparams_len, 0755);
	if (p->pki_dhparams == NULL) {
		if (errno == EACCES)
			return 0;
		log_info("info: No DH parameters found in %s: "
		    "using built-in parameters", pathname);
	}
	return 1;
}

const char *
ssl_to_text(const SSL *ssl)
{
	static char buf[256];

	(void)snprintf(buf, sizeof buf, "version=%s, cipher=%s, bits=%d",
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

	for (; (code = ERR_get_error()) != 0 ;) {
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

void
ssl_set_ecdh_curve(SSL_CTX *ctx, const char *curve)
{
	int	nid;
	EC_KEY *ecdh;

	if (curve == NULL)
		curve = SSL_ECDH_CURVE;
	if ((nid = OBJ_sn2nid(curve)) == 0) {
		ssl_error("ssl_set_ecdh_curve");
		fatal("ssl_set_ecdh_curve: unknown curve name "
		    SSL_ECDH_CURVE);
	}

	if ((ecdh = EC_KEY_new_by_curve_name(nid)) == NULL) {
		ssl_error("ssl_set_ecdh_curve");
		fatal("ssl_set_ecdh_curve: unable to create curve "
		    SSL_ECDH_CURVE);
	}

	SSL_CTX_set_tmp_ecdh(ctx, ecdh);
	SSL_CTX_set_options(ctx, SSL_OP_SINGLE_ECDH_USE);
	EC_KEY_free(ecdh);
}

int
ssl_load_pkey(const void *data, size_t datalen, char *buf, off_t len,
    X509 **x509ptr, EVP_PKEY **pkeyptr)
{
	BIO		*in;
	X509		*x509 = NULL;
	EVP_PKEY	*pkey = NULL;
	RSA		*rsa = NULL;
	void		*exdata = NULL;

	if ((in = BIO_new_mem_buf(buf, len)) == NULL) {
		SSLerr(SSL_F_SSL_CTX_USE_PRIVATEKEY, ERR_R_BUF_LIB);
		return (0);
	}

	if ((x509 = PEM_read_bio_X509(in, NULL,
	    ssl_password_cb, NULL)) == NULL) {
		SSLerr(SSL_F_SSL_CTX_USE_PRIVATEKEY, ERR_R_PEM_LIB);
		goto fail;
	}

	if ((pkey = X509_get_pubkey(x509)) == NULL) {
		SSLerr(SSL_F_SSL_CTX_USE_PRIVATEKEY, ERR_R_X509_LIB);
		goto fail;
	}

	BIO_free(in);
	in = NULL;

	if (data != NULL && datalen) {
		if ((rsa = EVP_PKEY_get1_RSA(pkey)) == NULL ||
		    (exdata = malloc(datalen)) == NULL) {
			SSLerr(SSL_F_SSL_CTX_USE_PRIVATEKEY, ERR_R_EVP_LIB);
			goto fail;
		}

		memcpy(exdata, data, datalen);
		RSA_set_ex_data(rsa, 0, exdata);
		RSA_free(rsa); /* dereference, will be cleaned up with pkey */
	}

	*x509ptr = x509;
	*pkeyptr = pkey;

	return (1);

 fail:
	if (rsa != NULL)
		RSA_free(rsa);
	if (in != NULL)
		BIO_free(in);
	if (pkey != NULL)
		EVP_PKEY_free(pkey);
	if (x509 != NULL)
		X509_free(x509);
	free(exdata);

	return (0);
}

int
ssl_ctx_fake_private_key(SSL_CTX *ctx, const void *data, size_t datalen,
    char *buf, off_t len, X509 **x509ptr, EVP_PKEY **pkeyptr)
{
	int		 ret = 0;
	EVP_PKEY	*pkey = NULL;
	X509		*x509 = NULL;

	if (!ssl_load_pkey(data, datalen, buf, len, &x509, &pkey))
		return (0);

	/*
	 * Use the public key as the "private" key - the secret key
	 * parameters are hidden in an extra process that will be
	 * contacted by the RSA engine.  The SSL/TLS library needs at
	 * least the public key parameters in the current process.
	 */
	ret = SSL_CTX_use_PrivateKey(ctx, pkey);
	if (!ret)
		SSLerr(SSL_F_SSL_CTX_USE_PRIVATEKEY, ERR_R_SSL_LIB);

	if (pkeyptr != NULL)
		*pkeyptr = pkey;
	else if (pkey != NULL)
		EVP_PKEY_free(pkey);

	if (x509ptr != NULL)
		*x509ptr = x509;
	else if (x509 != NULL)
		X509_free(x509);

	return (ret);
}
