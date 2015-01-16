/*	$OpenBSD: ssl.h,v 1.10 2015/01/16 15:08:52 reyk Exp $	*/
/*
 * Copyright (c) 2013 Gilles Chehade <gilles@poolp.org>
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

#define SSL_CIPHERS		"HIGH:!aNULL:!MD5"
#define	SSL_ECDH_CURVE		"prime256v1"
#define	SSL_SESSION_TIMEOUT	300

struct pki {
	char			 pki_name[PATH_MAX];

	char			*pki_ca_file;
	char			*pki_ca;
	off_t			 pki_ca_len;

	char			*pki_cert_file;
	char			*pki_cert;
	off_t			 pki_cert_len;

	char			*pki_key_file;
	char			*pki_key;
	off_t			 pki_key_len;

	EVP_PKEY		*pki_pkey;

	char			*pki_dhparams_file;
	char			*pki_dhparams;
	off_t			 pki_dhparams_len;
};

/* ssl.c */
void		ssl_init(void);
int		ssl_setup(SSL_CTX **, struct pki *);
SSL_CTX	       *ssl_ctx_create(const char *, char *, off_t);
int	        ssl_cmp(struct pki *, struct pki *);
DH	       *get_dh1024(void);
DH	       *get_dh_from_memory(char *, size_t);
void		ssl_set_ephemeral_key_exchange(SSL_CTX *, DH *);
void		ssl_set_ecdh_curve(SSL_CTX *, const char *);
extern int	ssl_ctx_load_verify_memory(SSL_CTX *, char *, off_t);
char	       *ssl_load_file(const char *, off_t *, mode_t);
char	       *ssl_load_key(const char *, off_t *, char *, mode_t, const char *);

const char     *ssl_to_text(const SSL *);
void		ssl_error(const char *);

int		ssl_load_certificate(struct pki *, const char *);
int		ssl_load_keyfile(struct pki *, const char *, const char *);
int		ssl_load_cafile(struct pki *, const char *);
int		ssl_load_dhparams(struct pki *, const char *);
int		ssl_load_pkey(const void *, size_t, char *, off_t,
		    X509 **, EVP_PKEY **);
int		ssl_ctx_fake_private_key(SSL_CTX *, const void *, size_t,
		    char *, off_t, X509 **, EVP_PKEY **);

/* ssl_privsep.c */
int		ssl_ctx_load_verify_memory(SSL_CTX *, char *, off_t);
int		ssl_by_mem_ctrl(X509_LOOKUP *, int, const char *, long, char **);
