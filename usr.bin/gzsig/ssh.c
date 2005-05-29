/* $OpenBSD: ssh.c,v 1.2 2005/05/29 09:10:23 djm Exp $ */

/*
 * ssh.c
 *
 * Copyright (c) 2001 Dug Song <dugsong@monkey.org>
 * Copyright (c) 2000 Niels Provos <provos@monkey.org>
 * Copyright (c) 2000 Markus Friedl <markus@monkey.org>
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. The names of the copyright holders may not be used to endorse or
 *      promote products derived from this software without specific
 *      prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 *   AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 *   THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Vendor: ssh.c,v 1.2 2005/04/01 16:47:31 dugsong Exp $
 */

#include <sys/types.h>
#include <sys/uio.h>

#include <arpa/nameser.h>
#include <openssl/ssl.h>
#include <openssl/des.h>
#include <openssl/md5.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "key.h"
#include "ssh.h"

#define SSH1_MAGIC		"SSH PRIVATE KEY FILE FORMAT 1.1\n"

extern int sign_passwd_cb(char *, int, int, void *);

struct des3_state {
	des_key_schedule	k1, k2, k3;
	des_cblock		iv1, iv2, iv3;
};

static int
get_bn(BIGNUM *bn, u_char **pp, int *lenp)
{
	short i;

	if (*lenp < 2) {
		errno = EINVAL;
		return (-1);
	}
	GETSHORT(i, *pp); *lenp -= 2;

	i = ((i + 7) / 8);

	if (*lenp < i) {
		errno = EINVAL;
		return (-1);
	}
	BN_bin2bn(*pp, i, bn);
	
	*pp += i; *lenp -= i;

	return (0);
}

static int
get_string(char *dst, int len, u_char **pp, int *lenp)
{
	long i;
	
	if (*lenp < 4) {
		errno = EINVAL;
		return (-1);
	}
	GETLONG(i, *pp); *lenp -= 4;

	if (*lenp < i || len < i) {
		errno = EINVAL;
		return (-1);
	}
	memcpy(dst, *pp, i);

	*pp += i; *lenp -= i;

	return (0);
}

static int
read_ssh1_bn(BIGNUM *value, char **cpp)
{
	char *cp = *cpp;
	int old;
	
	/* Skip any leading whitespace. */
	for (; *cp == ' ' || *cp == '\t'; cp++)
		;
	
	/* Check that it begins with a decimal digit. */
	if (*cp < '0' || *cp > '9') {
		errno = EINVAL;
		return (-1);
	}
	/* Save starting position. */
	*cpp = cp;
	
	/* Move forward until all decimal digits skipped. */
	for (; *cp >= '0' && *cp <= '9'; cp++)
		;
	
	/* Save the old terminating character, and replace it by \0. */
	old = *cp;
	*cp = 0;
	
	/* Parse the number. */
	if (BN_dec2bn(&value, *cpp) == 0)
		return (-1);
	
	/* Restore old terminating character. */
	*cp = old;
	
	/* Move beyond the number and return success. */
	*cpp = cp;
	return (0);
}

/* XXX - SSH1's weirdo 3DES... */
static void *
des3_init(u_char *sesskey, int len)
{
	struct des3_state *state;
	
	if ((state = malloc(sizeof(*state))) == NULL)
		return (NULL);

	des_set_key((void *)sesskey, state->k1);
	des_set_key((void *)(sesskey + 8), state->k2);

	if (len <= 16)
		des_set_key((void *)sesskey, state->k3);
	else
		des_set_key((void *)(sesskey + 16), state->k3);
	
	memset(state->iv1, 0, 8);
	memset(state->iv2, 0, 8);
	memset(state->iv3, 0, 8);
	
	return (state);
}

static void
des3_decrypt(u_char *src, u_char *dst, int len, void *state)
{
	struct des3_state *dstate;
	
	dstate = (struct des3_state *)state;
	memcpy(dstate->iv1, dstate->iv2, 8);
	
	des_ncbc_encrypt(src, dst, len, dstate->k3, &dstate->iv3, DES_DECRYPT);
	des_ncbc_encrypt(dst, dst, len, dstate->k2, &dstate->iv2, DES_ENCRYPT);
	des_ncbc_encrypt(dst, dst, len, dstate->k1, &dstate->iv1, DES_DECRYPT);
}

static int
load_ssh1_public(RSA *rsa, struct iovec *iov)
{
	char *p;
	u_int bits;

	/* Skip leading whitespace. */
	for (p = iov->iov_base; *p == ' ' || *p == '\t'; p++)
		;

	/* Get number of bits. */
	if (*p < '0' || *p > '9')
		return (-1);
	
	for (bits = 0; *p >= '0' && *p <= '9'; p++)
		bits = 10 * bits + *p - '0';

	if (bits == 0)
		return (-1);
	
	/* Get public exponent, public modulus. */
	if (read_ssh1_bn(rsa->e, &p) < 0)
		return (-1);
		
	if (read_ssh1_bn(rsa->n, &p) < 0)
		return (-1);

	return (0);
}

static int
load_ssh1_private(RSA *rsa, struct iovec *iov)
{
	BN_CTX *ctx;
	BIGNUM *aux;
	MD5_CTX md;
	char pass[128], comment[BUFSIZ];
	u_char *p, cipher_type, digest[16];
	void *dstate;
	int i;

	i = strlen(SSH1_MAGIC) + 1;

	/* Make sure it begins with the id string. */
	if (iov->iov_len < i || memcmp(iov->iov_base, SSH1_MAGIC, i) != 0)
		return (-1);
	
	p = (u_char *)iov->iov_base + i;
	i = iov->iov_len - i;
	
	/* Skip cipher_type, reserved data, bits. */
	cipher_type = *p;
	p += 1 + 4 + 4;
	i -= 1 + 4 + 4;

	/* Read public key. */
	if (get_bn(rsa->n, &p, &i) < 0 || get_bn(rsa->e, &p, &i) < 0)
		return (-1);
	
	/* Read comment. */
	if (get_string(comment, sizeof(comment), &p, &i) < 0)
		return (-1);
	
	/* Decrypt private key. */
	if (cipher_type != 0) {
		sign_passwd_cb(pass, sizeof(pass), 0, NULL);

		MD5_Init(&md);
		MD5_Update(&md, (const u_char *)pass, strlen(pass));
		MD5_Final(digest, &md);
		
		memset(pass, 0, strlen(pass));
		
		if ((dstate = des3_init(digest, sizeof(digest))) == NULL)
			return (-1);
		
		des3_decrypt(p, p, i, dstate);

		if (p[0] != p[2] || p[1] != p[3]) {
			fprintf(stderr, "Bad passphrase for %s\n", comment);
			return (-1);
		}
	}
	else if (p[0] != p[2] || p[1] != p[3])
		return (-1);
	
	p += 4;
	i -= 4;
	
	/* Read the private key. */
	if (get_bn(rsa->d, &p, &i) < 0 ||
	    get_bn(rsa->iqmp, &p, &i) < 0)
		return (-1);
	
	/* In SSL and SSH v1 p and q are exchanged. */
	if (get_bn(rsa->q, &p, &i) < 0 ||
	    get_bn(rsa->p, &p, &i) < 0)
		return (-1);
	
	/* Calculate p-1 and q-1. */
	ctx = BN_CTX_new();
	aux = BN_new();

	BN_sub(aux, rsa->q, BN_value_one());
	BN_mod(rsa->dmq1, rsa->d, aux, ctx);

	BN_sub(aux, rsa->p, BN_value_one());
	BN_mod(rsa->dmp1, rsa->d, aux, ctx);

	BN_clear_free(aux);
	BN_CTX_free(ctx);
	
	return (0);
}

int
ssh_load_public(struct key *k, struct iovec *iov)
{
	RSA *rsa;
	
	rsa = RSA_new();

	rsa->n = BN_new();
	rsa->e = BN_new();

	if (load_ssh1_public(rsa, iov) < 0) {
		RSA_free(rsa);
		return (-1);
	}
	k->type = KEY_RSA;
	k->data = (void *)rsa;
	
	return (0);
}

int
ssh_load_private(struct key *k, struct iovec *iov)
{
	RSA *rsa;
	
	rsa = RSA_new();

	rsa->n = BN_new();
	rsa->e = BN_new();
	
	rsa->d = BN_new();
	rsa->iqmp = BN_new();
	rsa->q = BN_new();
	rsa->p = BN_new();
	rsa->dmq1 = BN_new();
	rsa->dmp1 = BN_new();
	
	if (load_ssh1_private(rsa, iov) < 0) {
		RSA_free(rsa);
		return (-1);

	}
	k->type = KEY_RSA;
	k->data = (void *)rsa;
	
	return (0);
}
