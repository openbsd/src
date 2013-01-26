/*	$OpenBSD: ssl.h,v 1.1 2013/01/26 09:37:24 gilles Exp $	*/
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

#define SSL_CIPHERS		"HIGH"
#define	SSL_SESSION_TIMEOUT	300

struct ssl {
	char			 ssl_name[PATH_MAX];
	char			*ssl_ca;
	off_t			 ssl_ca_len;
	char			*ssl_cert;
	off_t			 ssl_cert_len;
	char			*ssl_key;
	off_t			 ssl_key_len;
	char			*ssl_dhparams;
	off_t			 ssl_dhparams_len;
	uint8_t			 flags;
};

/* ssl.c */
void		ssl_init(void);
int		ssl_setup(SSL_CTX **, struct ssl *);
SSL_CTX	       *ssl_ctx_create(void);
int		ssl_load_certfile(struct ssl **, const char *, const char *, uint8_t);
void	       *ssl_mta_init(char *, off_t, char *, off_t);
void	       *ssl_smtp_init(void *, char *, off_t, char *, off_t);
int	        ssl_cmp(struct ssl *, struct ssl *);
DH	       *get_dh1024(void);
DH	       *get_dh_from_memory(char *, size_t);
void		ssl_set_ephemeral_key_exchange(SSL_CTX *, DH *);
extern int	ssl_ctx_load_verify_memory(SSL_CTX *, char *, off_t);
char	       *ssl_load_file(const char *, off_t *, mode_t);
char	       *ssl_load_key(const char *, off_t *, char *);

const char     *ssl_to_text(const SSL *);
void		ssl_error(const char *);


/* ssl_privsep.c */
int	 ssl_ctx_use_private_key(SSL_CTX *, char *, off_t);
int	 ssl_ctx_use_certificate_chain(SSL_CTX *, char *, off_t);
int      ssl_ctx_load_verify_memory(SSL_CTX *, char *, off_t);
int      ssl_by_mem_ctrl(X509_LOOKUP *, int, const char *, long, char **);
