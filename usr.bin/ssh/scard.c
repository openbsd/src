/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
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

#ifdef SMARTCARD
#include "includes.h"
RCSID("$OpenBSD: scard.c,v 1.1 2001/06/26 05:33:34 markus Exp $");

#include <openssl/engine.h>
#include <sectok.h>

#include "key.h"
#include "log.h"
#include "xmalloc.h"
#include "scard.h"

#define CLA_SSH 0x05
#define INS_DECRYPT 0x10
#define INS_GET_KEYLENGTH 0x20
#define INS_GET_PUBKEY 0x30
#define INS_GET_RESPONSE 0xc0

#define MAX_BUF_SIZE 256

static int sc_fd = -1;
static int sc_reader_num = 0;
static int cla = 0x00;	/* class */

/* interface to libsectok */

static int 
sc_open(int num)
{
	int n;
	u_char atr[256];

	if (sc_fd >= 0)
		return sc_fd;
	sc_reader_num = num;

	sc_fd = scopen(sc_reader_num, 0, NULL);
	if (sc_fd < 0) {
		error("scopen failed %d", sc_fd);
		return sc_fd;
	}
	n = screset(sc_fd, atr, NULL);
	if (n <= 0) {
		error("screset failed.");
		sc_fd = -1;
		return sc_fd;
	}
	debug("open ok %d", sc_fd);
	return sc_fd;
}

static int 
sc_reset(void)
{
	scclose(sc_fd);
	sc_fd = -1;
	return sc_open(sc_reader_num);
}

static int
selectfile(int fd, int f0, int f1, int verbose)
{
	int n, r1, r2, code;
	u_char buf[2], obuf[256];

	buf[0] = f0;
	buf[1] = f1;
	n = scrw(sc_fd, cla, 0xa4, 0, 0, 2, buf, sizeof obuf, obuf, &r1, &r2);
	if (n < 0) {
		error("selectfile: scwrite failed");
		return -2;
	}
	if (r1 == 0x90 || r1 == 0x61)
		code = 0;
	else if (r1 == 0x6a && r2 == 0x82)
		/* file not found */
		code = -1;
	else
		code = -2;
	if (verbose && n > 0)
		dump_reply(obuf, n, 0, 0);
	if (verbose || code == -2) {
		error("%x.%x: %s", f0, f1, get_r1r2s(r1, r2));
	}
	return code;
}

static int 
sc_enable_applet(void)
{
	u_char data[MAX_BUF_SIZE];
	u_char progID[2], contID[2], aid[MAX_BUF_SIZE];
	int i, len, rv, r1, r2, aid_len;

	len = rv = r1 = r2 = 0;
	progID[0] = 0x77;
	progID[1] = 0x77;
	contID[0] = 0x77;
	contID[1] = 0x78;
	aid_len = 5;

	for (i = 0; i < 16; i++)
		aid[i] = 0x77;

	rv = selectfile(sc_fd, contID[0], contID[1], 0);
	if (rv < 0) {
		error("selectfile failed");
		return -1;
	}
	for (i = 0; i < aid_len; i++)
		data[i] = (u_char) aid[i];
	rv = scwrite(sc_fd, cla, 0xa4, 0x04, 0, aid_len, data, &r1, &r2);
	if (r1 != 0x90 && r1 != 0x61) {
		/* error */
		error("selecting the cardlet: ");
		for (i = 0; i < aid_len; i++) {
			error("%02x", (u_char) aid[i]);
		}
		print_r1r2(r1, r2);
		return -1;
	}
	return 0;
}

static int 
sc_read_pubkey(Key * k)
{
	u_char          buf[256];
	char           *p;
	int             len, rv, r1, r2;

	len = rv = r1 = r2 = 0;

	/* get key size */
	rv = scread(sc_fd, CLA_SSH, INS_GET_KEYLENGTH, 0, 0, 2, buf, &r1, &r2);
	if (rv < 0) {
		error("could not obtain key length.");
		return rv;
	}
	len = (buf[0] << 8) | buf[1];
	error("len %d r1 %d r2 %d", len, r1, r2);
	len /= 8;

	/* get n */
	rv = scread(sc_fd, CLA_SSH, INS_GET_PUBKEY, 0, 0, len, buf, &r1, &r2);
	if (rv < 0) {
		error("could not obtain public key");
		return rv;
	}
	debug("len %d r1 %d r2 %d", len, r1, r2);
	BN_bin2bn(buf, len, k->rsa->n);

	/* currently the java applet just stores 'n' */
	BN_set_word(k->rsa->e, 35);	/* XXX */

	p = key_fingerprint(k, SSH_FP_MD5, SSH_FP_HEX);
	debug("fingerprint %d %s", key_size(k), p);
	xfree(p);

	return 0;
}

/* private key operations */

static int
sc_private_decrypt(int flen, unsigned char *from,
    unsigned char *to, RSA *rsa, int padding)
{
        int rv, num, r1, r2, olen;
        u_char *padded = NULL;

	debug("sc_private_decrypt called");

	olen = num = r1 = r2 = 0;
        if (padding != RSA_PKCS1_PADDING)
		goto err;

        num = BN_num_bytes(rsa->n);
	padded = xmalloc(num);

	rv = scwrite(sc_fd, CLA_SSH, INS_DECRYPT, 0, 0, num, from, &r1, &r2);
	if (rv < 0) {
		error("scwrite() for decrypt failed.");
		goto err;
	}
	if (r1 != 0x90 && r1 != 0x61) {
		error("INS_DECRYPT: r1 %x r2 %x", r1, r2);
                goto err;
	}
	rv = scread(sc_fd, CLA_SSH, INS_GET_RESPONSE, 0, 0, num, padded, &r1, &r2);
	if (rv < 0) {
		error("scread() for decrypt failed");
		goto err;
	}
	if (r1 != 0x90 && r1 != 0x61) {
		error("INS_GET_RESPONSE: r1 %x r2 %x", r1, r2);
                goto err;
	}
	debug("r1 %x r2 %x", r1, r2);
	olen = RSA_padding_check_PKCS1_type_2(to, num, padded + 1, num - 1, num);
err:
	if (padded)
		xfree(padded);
	return olen;
}


static int
sc_private_encrypt(int flen, unsigned char *from,
    unsigned char *to, RSA *rsa, int padding)
{
        int rv, i, num, r1, r2;
        u_char *padded = NULL;

	num = r1 = r2 = 0;
        if (padding != RSA_PKCS1_PADDING)
		goto err;

	error("sc_private_encrypt called");
        num = BN_num_bytes(rsa->n);
	padded = xmalloc(num);
	i = RSA_padding_add_PKCS1_type_1(padded, num, from, flen);
	if (i <= 0) {
		error("RSA_padding_add_PKCS1_type_1 failed");
		goto err;
	}
        rv = scwrite(sc_fd, CLA_SSH, INS_DECRYPT, 0, 0, num, padded, &r1, &r2);
        if (rv < 0) {
                error("scwrite() for rsa failed");
		sc_reset();
                goto err;
        }
	if (r1 != 0x90 && r1 != 0x61) {
		error("INS_DECRYPT: r1 %x r2 %x", r1, r2);
                goto err;
	}
        rv = scread(sc_fd, CLA_SSH, INS_GET_RESPONSE, 0, 0, num, to, &r1, &r2);
        if (rv < 0) {
                error("scread() for rsa failed");
		sc_reset();
                goto err;
        }
	if (r1 != 0x90 && r1 != 0x61) {
		error("INS_GET_RESPONSE: r1 %x r2 %x", r1, r2);
                goto err;
	}
err:
	if (padded)
		xfree(padded);
	return num;
}

/* engine for overloading private key operations */

static ENGINE *smart_engine = NULL;
static RSA_METHOD smart_rsa =
{
        "smartcard",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        NULL,
};

ENGINE *
sc_get_engine()
{
	RSA_METHOD *def;

	def = RSA_get_default_openssl_method();

	/* overload */
	smart_rsa.rsa_priv_enc	= sc_private_encrypt;
	smart_rsa.rsa_priv_dec	= sc_private_decrypt;

	/* just use the OpenSSL version */
        smart_rsa.rsa_pub_enc   = def->rsa_pub_enc;
        smart_rsa.rsa_pub_dec   = def->rsa_pub_dec;
	smart_rsa.rsa_mod_exp	= def->rsa_mod_exp;
	smart_rsa.bn_mod_exp	= def->bn_mod_exp;
	smart_rsa.init		= def->init;
	smart_rsa.finish	= def->finish;
	smart_rsa.flags		= def->flags;
	smart_rsa.app_data	= def->app_data;
	smart_rsa.rsa_sign	= def->rsa_sign;
	smart_rsa.rsa_verify	= def->rsa_verify;

	smart_engine = ENGINE_new();

	ENGINE_set_id(smart_engine, "xxx");
	ENGINE_set_name(smart_engine, "xxx");
	ENGINE_set_RSA(smart_engine, &smart_rsa);
	ENGINE_set_DSA(smart_engine, DSA_get_default_openssl_method());
	ENGINE_set_DH(smart_engine, DH_get_default_openssl_method());
	ENGINE_set_RAND(smart_engine, RAND_SSLeay());
	ENGINE_set_BN_mod_exp(smart_engine, BN_mod_exp);

	return smart_engine;
}

Key *
sc_get_key(int sc_reader_num)
{
	Key *k;
	int rv;

	rv = sc_open (sc_reader_num);
	if (rv < 0) {
		error("sc_open failed");
		return NULL;
	}
	rv = sc_enable_applet();
	if (rv < 0) {
		error("sc_enable_applet failed");
		return NULL;
	}
	k = key_new(KEY_RSA);
	if (k == NULL) {
		return NULL;
	}
	rv = sc_read_pubkey (k);
	if (rv < 0) {
		error("sc_read_pubkey failed");
		key_free(k);
		return NULL;
	}
	return k;
}
#endif
