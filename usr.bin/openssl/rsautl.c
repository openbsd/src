/* $OpenBSD: rsautl.c,v 1.24 2023/07/23 11:39:29 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <openssl/opensslconf.h>

#include <string.h>

#include "apps.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#define RSA_SIGN	1
#define RSA_VERIFY	2
#define RSA_ENCRYPT	3
#define RSA_DECRYPT	4

#define KEY_PRIVKEY	1
#define KEY_PUBKEY	2
#define KEY_CERT	3

static struct {
	int asn1parse;
	int hexdump;
	char *infile;
	char *keyfile;
	int keyform;
	int key_type;
	char *outfile;
	int pad;
	char *passargin;
	int rev;
	int rsa_mode;
} cfg;

static const struct option rsautl_options[] = {
	{
		.name = "asn1parse",
		.desc = "ASN.1 parse the output data",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.asn1parse,
	},
	{
		.name = "certin",
		.desc = "Input is a certificate containing an RSA public key",
		.type = OPTION_VALUE,
		.value = KEY_CERT,
		.opt.value = &cfg.key_type,
	},
	{
		.name = "decrypt",
		.desc = "Decrypt the input data using RSA private key",
		.type = OPTION_VALUE,
		.value = RSA_DECRYPT,
		.opt.value = &cfg.rsa_mode,
	},
	{
		.name = "encrypt",
		.desc = "Encrypt the input data using RSA public key",
		.type = OPTION_VALUE,
		.value = RSA_ENCRYPT,
		.opt.value = &cfg.rsa_mode,
	},
	{
		.name = "hexdump",
		.desc = "Hex dump the output data",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.hexdump,
	},
	{
		.name = "in",
		.argname = "file",
		.desc = "Input file (default stdin)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.infile,
	},
	{
		.name = "inkey",
		.argname = "file",
		.desc = "Input key file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.keyfile,
	},
	{
		.name = "keyform",
		.argname = "fmt",
		.desc = "Input key format (DER, TXT or PEM (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.keyform,
	},
	{
		.name = "oaep",
		.desc = "Use PKCS#1 OAEP padding",
		.type = OPTION_VALUE,
		.value = RSA_PKCS1_OAEP_PADDING,
		.opt.value = &cfg.pad,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output file (default stdout)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.outfile,
	},
	{
		.name = "passin",
		.argname = "arg",
		.desc = "Key password source",
		.type = OPTION_ARG,
		.opt.arg = &cfg.passargin,
	},
	{
		.name = "pkcs",
		.desc = "Use PKCS#1 v1.5 padding (default)",
		.type = OPTION_VALUE,
		.value = RSA_PKCS1_PADDING,
		.opt.value = &cfg.pad,
	},
	{
		.name = "pubin",
		.desc = "Input is an RSA public key",
		.type = OPTION_VALUE,
		.value = KEY_PUBKEY,
		.opt.value = &cfg.key_type,
	},
	{
		.name = "raw",
		.desc = "Use no padding",
		.type = OPTION_VALUE,
		.value = RSA_NO_PADDING,
		.opt.value = &cfg.pad,
	},
	{
		.name = "rev",
		.desc = "Reverse the input data",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.rev,
	},
	{
		.name = "sign",
		.desc = "Sign the input data using RSA private key",
		.type = OPTION_VALUE,
		.value = RSA_SIGN,
		.opt.value = &cfg.rsa_mode,
	},
	{
		.name = "verify",
		.desc = "Verify the input data using RSA public key",
		.type = OPTION_VALUE,
		.value = RSA_VERIFY,
		.opt.value = &cfg.rsa_mode,
	},
	{
		.name = "x931",
		.desc = "Use ANSI X9.31 padding",
		.type = OPTION_VALUE,
		.value = RSA_X931_PADDING,
		.opt.value = &cfg.pad,
	},

	{NULL},
};

static void
rsautl_usage(void)
{
	fprintf(stderr,
	    "usage: rsautl [-asn1parse] [-certin] [-decrypt] [-encrypt] "
	    "[-hexdump]\n"
	    "    [-in file] [-inkey file] [-keyform der | pem]\n"
	    "    [-oaep | -pkcs | -raw | -x931] [-out file] [-passin arg]\n"
	    "    [-pubin] [-rev] [-sign] [-verify]\n\n");

	options_usage(rsautl_options);
}

int
rsautl_main(int argc, char **argv)
{
	BIO *in = NULL, *out = NULL;
	X509 *x;
	EVP_PKEY *pkey = NULL;
	RSA *rsa = NULL;
	unsigned char *rsa_in = NULL, *rsa_out = NULL;
	char *passin = NULL;
	int rsa_inlen, rsa_outlen = 0;
	int need_priv = 0;
	int keysize;
	int ret = 1;

	if (pledge("stdio cpath wpath rpath tty", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.keyform = FORMAT_PEM;
	cfg.key_type = KEY_PRIVKEY;
	cfg.pad = RSA_PKCS1_PADDING;
	cfg.rsa_mode = RSA_VERIFY;

	if (options_parse(argc, argv, rsautl_options, NULL, NULL) != 0) {
		rsautl_usage();
		return (1);
	}

	if (cfg.rsa_mode == RSA_SIGN ||
	    cfg.rsa_mode == RSA_DECRYPT)
		need_priv = 1;

	if (need_priv && cfg.key_type != KEY_PRIVKEY) {
		BIO_printf(bio_err, "A private key is needed for this operation\n");
		goto end;
	}
	if (!app_passwd(bio_err, cfg.passargin, NULL, &passin, NULL)) {
		BIO_printf(bio_err, "Error getting password\n");
		goto end;
	}

	switch (cfg.key_type) {
	case KEY_PRIVKEY:
		pkey = load_key(bio_err, cfg.keyfile,
		    cfg.keyform, 0, passin, "Private Key");
		break;

	case KEY_PUBKEY:
		pkey = load_pubkey(bio_err, cfg.keyfile,
		    cfg.keyform, 0, NULL, "Public Key");
		break;

	case KEY_CERT:
		x = load_cert(bio_err, cfg.keyfile,
		    cfg.keyform, NULL, "Certificate");
		if (x) {
			pkey = X509_get_pubkey(x);
			X509_free(x);
		}
		break;
	}

	if (!pkey)
		goto end;

	rsa = EVP_PKEY_get1_RSA(pkey);
	EVP_PKEY_free(pkey);

	if (!rsa) {
		BIO_printf(bio_err, "Error getting RSA key\n");
		ERR_print_errors(bio_err);
		goto end;
	}
	if (cfg.infile) {
		if (!(in = BIO_new_file(cfg.infile, "rb"))) {
			BIO_printf(bio_err, "Error Reading Input File\n");
			ERR_print_errors(bio_err);
			goto end;
		}
	} else
		in = BIO_new_fp(stdin, BIO_NOCLOSE);

	if (cfg.outfile) {
		if (!(out = BIO_new_file(cfg.outfile, "wb"))) {
			BIO_printf(bio_err, "Error Reading Output File\n");
			ERR_print_errors(bio_err);
			goto end;
		}
	} else {
		out = BIO_new_fp(stdout, BIO_NOCLOSE);
	}

	keysize = RSA_size(rsa);

	rsa_in = reallocarray(NULL, keysize, 2);
	if (rsa_in == NULL) {
		BIO_printf(bio_err, "Error allocating memory for input data\n");
		exit(1);
	}
	rsa_out = malloc(keysize);
	if (rsa_out == NULL) {
		BIO_printf(bio_err, "Error allocating memory for output data\n");
		exit(1);
	}

	/* Read the input data */
	rsa_inlen = BIO_read(in, rsa_in, keysize * 2);
	if (rsa_inlen <= 0) {
		BIO_printf(bio_err, "Error reading input Data\n");
		exit(1);
	}
	if (cfg.rev) {
		int i;
		unsigned char ctmp;
		for (i = 0; i < rsa_inlen / 2; i++) {
			ctmp = rsa_in[i];
			rsa_in[i] = rsa_in[rsa_inlen - 1 - i];
			rsa_in[rsa_inlen - 1 - i] = ctmp;
		}
	}

	switch (cfg.rsa_mode) {
	case RSA_VERIFY:
		rsa_outlen = RSA_public_decrypt(rsa_inlen, rsa_in, rsa_out,
		    rsa, cfg.pad);
		break;

	case RSA_SIGN:
		rsa_outlen = RSA_private_encrypt(rsa_inlen, rsa_in, rsa_out,
		    rsa, cfg.pad);
		break;

	case RSA_ENCRYPT:
		rsa_outlen = RSA_public_encrypt(rsa_inlen, rsa_in, rsa_out,
		    rsa, cfg.pad);
		break;

	case RSA_DECRYPT:
		rsa_outlen = RSA_private_decrypt(rsa_inlen, rsa_in, rsa_out,
		    rsa, cfg.pad);
		break;
	}

	if (rsa_outlen <= 0) {
		BIO_printf(bio_err, "RSA operation error\n");
		ERR_print_errors(bio_err);
		goto end;
	}
	ret = 0;
	if (cfg.asn1parse) {
		if (!ASN1_parse_dump(out, rsa_out, rsa_outlen, 1, -1)) {
			ERR_print_errors(bio_err);
		}
	} else if (cfg.hexdump)
		BIO_dump(out, (char *) rsa_out, rsa_outlen);
	else
		BIO_write(out, rsa_out, rsa_outlen);

 end:
	RSA_free(rsa);
	BIO_free(in);
	BIO_free_all(out);
	free(rsa_in);
	free(rsa_out);
	free(passin);

	return ret;
}
