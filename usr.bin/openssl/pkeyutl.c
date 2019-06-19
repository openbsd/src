/* $OpenBSD: pkeyutl.c,v 1.15 2019/02/17 15:01:08 inoguchi Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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

#include <string.h>

#include "apps.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#define KEY_PRIVKEY	1
#define KEY_PUBKEY	2
#define KEY_CERT	3

struct {
	int asn1parse;
	EVP_PKEY_CTX *ctx;
	int hexdump;
	char *infile;
	int key_type;
	int keyform;
	int keysize;
	char *outfile;
	char *passargin;
	int peerform;
	int pkey_op;
	int rev;
	char *sigfile;
} pkeyutl_config;

static void pkeyutl_usage(void);

static int init_ctx(char *keyfile);

static int setup_peer(char *file);

static int pkeyutl_pkeyopt(char *pkeyopt);

static int do_keyop(EVP_PKEY_CTX * ctx, int pkey_op,
    unsigned char *out, size_t * poutlen,
    unsigned char *in, size_t inlen);

struct option pkeyutl_options[] = {
	{
		.name = "asn1parse",
		.desc = "ASN.1 parse the output data",
		.type = OPTION_FLAG,
		.opt.flag = &pkeyutl_config.asn1parse,
	},
	{
		.name = "certin",
		.desc = "Input is a certificate containing a public key",
		.type = OPTION_VALUE,
		.value = KEY_CERT,
		.opt.value = &pkeyutl_config.key_type,
	},
	{
		.name = "decrypt",
		.desc = "Decrypt the input data using a private key",
		.type = OPTION_VALUE,
		.value = EVP_PKEY_OP_DECRYPT,
		.opt.value = &pkeyutl_config.pkey_op,
	},
	{
		.name = "derive",
		.desc = "Derive a shared secret using the peer key",
		.type = OPTION_VALUE,
		.value = EVP_PKEY_OP_DERIVE,
		.opt.value = &pkeyutl_config.pkey_op,
	},
	{
		.name = "encrypt",
		.desc = "Encrypt the input data using a public key",
		.type = OPTION_VALUE,
		.value = EVP_PKEY_OP_ENCRYPT,
		.opt.value = &pkeyutl_config.pkey_op,
	},
	{
		.name = "hexdump",
		.desc = "Hex dump the output data",
		.type = OPTION_FLAG,
		.opt.flag = &pkeyutl_config.hexdump,
	},
	{
		.name = "in",
		.argname = "file",
		.desc = "Input file (default stdin)",
		.type = OPTION_ARG,
		.opt.arg = &pkeyutl_config.infile,
	},
	{
		.name = "inkey",
		.argname = "file",
		.desc = "Input key file",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = init_ctx,
	},
	{
		.name = "keyform",
		.argname = "fmt",
		.desc = "Input key format (DER or PEM (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &pkeyutl_config.keyform,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output file (default stdout)",
		.type = OPTION_ARG,
		.opt.arg = &pkeyutl_config.outfile,
	},
	{
		.name = "passin",
		.argname = "arg",
		.desc = "Key password source",
		.type = OPTION_ARG,
		.opt.arg = &pkeyutl_config.passargin,
	},
	{
		.name = "peerform",
		.argname = "fmt",
		.desc = "Input key format (DER or PEM (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &pkeyutl_config.peerform,
	},
	{
		.name = "peerkey",
		.argname = "file",
		.desc = "Peer key file",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = setup_peer,
	},
	{
		.name = "pkeyopt",
		.argname = "opt:value",
		.desc = "Public key options",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = pkeyutl_pkeyopt,
	},
	{
		.name = "pubin",
		.desc = "Input is a public key",
		.type = OPTION_VALUE,
		.value = KEY_PUBKEY,
		.opt.value = &pkeyutl_config.key_type,
	},
	{
		.name = "rev",
		.desc = "Reverse the input data",
		.type = OPTION_FLAG,
		.opt.flag = &pkeyutl_config.rev,
	},
	{
		.name = "sigfile",
		.argname = "file",
		.desc = "Signature file (verify operation only)",
		.type = OPTION_ARG,
		.opt.arg = &pkeyutl_config.sigfile,
	},
	{
		.name = "sign",
		.desc = "Sign the input data using private key",
		.type = OPTION_VALUE,
		.value = EVP_PKEY_OP_SIGN,
		.opt.value = &pkeyutl_config.pkey_op,
	},
	{
		.name = "verify",
		.desc = "Verify the input data using public key",
		.type = OPTION_VALUE,
		.value = EVP_PKEY_OP_VERIFY,
		.opt.value = &pkeyutl_config.pkey_op,
	},
	{
		.name = "verifyrecover",
		.desc = "Verify with public key, recover original data",
		.type = OPTION_VALUE,
		.value = EVP_PKEY_OP_VERIFYRECOVER,
		.opt.value = &pkeyutl_config.pkey_op,
	},

	{NULL},
};

static void
pkeyutl_usage()
{
	fprintf(stderr,
	    "usage: pkeyutl [-asn1parse] [-certin] [-decrypt] [-derive] "
	    "[-encrypt]\n"
	    "    [-hexdump] [-in file] [-inkey file] [-keyform fmt]\n"
	    "    [-out file] [-passin arg] [-peerform fmt]\n"
	    "    [-peerkey file] [-pkeyopt opt:value] [-pubin] [-rev]\n"
	    "    [-sigfile file] [-sign] [-verify] [-verifyrecover]\n\n");
	options_usage(pkeyutl_options);
        fprintf(stderr, "\n");
}

int
pkeyutl_main(int argc, char **argv)
{
	BIO *in = NULL, *out = NULL;

	unsigned char *buf_in = NULL, *buf_out = NULL, *sig = NULL;
	size_t buf_outlen = 0;
	int buf_inlen = 0, siglen = -1;

	int ret = 1, rv = -1;

	if (single_execution) {
		if (pledge("stdio cpath wpath rpath tty", NULL) == -1) {
			perror("pledge");
			exit(1);
		}
	}

	memset(&pkeyutl_config, 0, sizeof(pkeyutl_config));
	pkeyutl_config.pkey_op = EVP_PKEY_OP_SIGN;
	pkeyutl_config.key_type = KEY_PRIVKEY;
	pkeyutl_config.keyform = FORMAT_PEM;
	pkeyutl_config.peerform = FORMAT_PEM;
	pkeyutl_config.keysize = -1;

	if (options_parse(argc, argv, pkeyutl_options, NULL, NULL) != 0) {
		pkeyutl_usage();
		goto end;
	}

	if (!pkeyutl_config.ctx) {
		pkeyutl_usage();
		goto end;
	}
	if (pkeyutl_config.sigfile &&
	    (pkeyutl_config.pkey_op != EVP_PKEY_OP_VERIFY)) {
		BIO_puts(bio_err, "Signature file specified for non verify\n");
		goto end;
	}
	if (!pkeyutl_config.sigfile &&
	    (pkeyutl_config.pkey_op == EVP_PKEY_OP_VERIFY)) {
		BIO_puts(bio_err, "No signature file specified for verify\n");
		goto end;
	}

	if (pkeyutl_config.pkey_op != EVP_PKEY_OP_DERIVE) {
		if (pkeyutl_config.infile) {
			if (!(in = BIO_new_file(pkeyutl_config.infile, "rb"))) {
				BIO_puts(bio_err,
				    "Error Opening Input File\n");
				ERR_print_errors(bio_err);
				goto end;
			}
		} else
			in = BIO_new_fp(stdin, BIO_NOCLOSE);
	}
	if (pkeyutl_config.outfile) {
		if (!(out = BIO_new_file(pkeyutl_config.outfile, "wb"))) {
			BIO_printf(bio_err, "Error Creating Output File\n");
			ERR_print_errors(bio_err);
			goto end;
		}
	} else {
		out = BIO_new_fp(stdout, BIO_NOCLOSE);
	}

	if (pkeyutl_config.sigfile) {
		BIO *sigbio = BIO_new_file(pkeyutl_config.sigfile, "rb");
		if (!sigbio) {
			BIO_printf(bio_err, "Can't open signature file %s\n",
			    pkeyutl_config.sigfile);
			goto end;
		}
		siglen = bio_to_mem(&sig, pkeyutl_config.keysize * 10, sigbio);
		BIO_free(sigbio);
		if (siglen <= 0) {
			BIO_printf(bio_err, "Error reading signature data\n");
			goto end;
		}
	}
	if (in) {
		/* Read the input data */
		buf_inlen = bio_to_mem(&buf_in, pkeyutl_config.keysize * 10, in);
		if (buf_inlen <= 0) {
			BIO_printf(bio_err, "Error reading input Data\n");
			exit(1);
		}
		if (pkeyutl_config.rev) {
			size_t i;
			unsigned char ctmp;
			size_t l = (size_t) buf_inlen;
			for (i = 0; i < l / 2; i++) {
				ctmp = buf_in[i];
				buf_in[i] = buf_in[l - 1 - i];
				buf_in[l - 1 - i] = ctmp;
			}
		}
	}
	if (pkeyutl_config.pkey_op == EVP_PKEY_OP_VERIFY) {
		rv = EVP_PKEY_verify(pkeyutl_config.ctx, sig, (size_t) siglen,
		    buf_in, (size_t) buf_inlen);
		if (rv == 1) {
			BIO_puts(out, "Signature Verified Successfully\n");
			ret = 0;
		} else
			BIO_puts(out, "Signature Verification Failure\n");
		if (rv >= 0)
			goto end;
	} else {
		rv = do_keyop(pkeyutl_config.ctx, pkeyutl_config.pkey_op, NULL,
		    (size_t *)&buf_outlen, buf_in, (size_t) buf_inlen);
		if (rv > 0) {
			buf_out = malloc(buf_outlen);
			if (!buf_out)
				rv = -1;
			else
				rv = do_keyop(pkeyutl_config.ctx,
				    pkeyutl_config.pkey_op,
				    buf_out, (size_t *) & buf_outlen,
				    buf_in, (size_t) buf_inlen);
		}
	}

	if (rv <= 0) {
		BIO_printf(bio_err, "Public Key operation error\n");
		ERR_print_errors(bio_err);
		goto end;
	}
	ret = 0;
	if (pkeyutl_config.asn1parse) {
		if (!ASN1_parse_dump(out, buf_out, buf_outlen, 1, -1))
			ERR_print_errors(bio_err);
	} else if (pkeyutl_config.hexdump)
		BIO_dump(out, (char *) buf_out, buf_outlen);
	else
		BIO_write(out, buf_out, buf_outlen);

 end:
	EVP_PKEY_CTX_free(pkeyutl_config.ctx);
	BIO_free(in);
	BIO_free_all(out);
	free(buf_in);
	free(buf_out);
	free(sig);

	return ret;
}

static int
init_ctx(char *keyfile)
{
	EVP_PKEY *pkey = NULL;
	char *passin = NULL;
	int rv = -1;
	X509 *x;

	if (((pkeyutl_config.pkey_op == EVP_PKEY_OP_SIGN)
		|| (pkeyutl_config.pkey_op == EVP_PKEY_OP_DECRYPT)
		|| (pkeyutl_config.pkey_op == EVP_PKEY_OP_DERIVE))
	    && (pkeyutl_config.key_type != KEY_PRIVKEY)) {
		BIO_printf(bio_err,
		    "A private key is needed for this operation\n");
		goto end;
	}
	if (!app_passwd(bio_err, pkeyutl_config.passargin, NULL, &passin,
	    NULL)) {
		BIO_printf(bio_err, "Error getting password\n");
		goto end;
	}
	switch (pkeyutl_config.key_type) {
	case KEY_PRIVKEY:
		pkey = load_key(bio_err, keyfile, pkeyutl_config.keyform, 0,
		    passin, "Private Key");
		break;

	case KEY_PUBKEY:
		pkey = load_pubkey(bio_err, keyfile, pkeyutl_config.keyform, 0,
		    NULL, "Public Key");
		break;

	case KEY_CERT:
		x = load_cert(bio_err, keyfile, pkeyutl_config.keyform,
		    NULL, "Certificate");
		if (x) {
			pkey = X509_get_pubkey(x);
			X509_free(x);
		}
		break;
	}

	pkeyutl_config.keysize = EVP_PKEY_size(pkey);

	if (!pkey)
		goto end;

	pkeyutl_config.ctx = EVP_PKEY_CTX_new(pkey, NULL);

	EVP_PKEY_free(pkey);

	if (!pkeyutl_config.ctx)
		goto end;

	switch (pkeyutl_config.pkey_op) {
	case EVP_PKEY_OP_SIGN:
		rv = EVP_PKEY_sign_init(pkeyutl_config.ctx);
		break;

	case EVP_PKEY_OP_VERIFY:
		rv = EVP_PKEY_verify_init(pkeyutl_config.ctx);
		break;

	case EVP_PKEY_OP_VERIFYRECOVER:
		rv = EVP_PKEY_verify_recover_init(pkeyutl_config.ctx);
		break;

	case EVP_PKEY_OP_ENCRYPT:
		rv = EVP_PKEY_encrypt_init(pkeyutl_config.ctx);
		break;

	case EVP_PKEY_OP_DECRYPT:
		rv = EVP_PKEY_decrypt_init(pkeyutl_config.ctx);
		break;

	case EVP_PKEY_OP_DERIVE:
		rv = EVP_PKEY_derive_init(pkeyutl_config.ctx);
		break;
	}

	if (rv <= 0) {
		EVP_PKEY_CTX_free(pkeyutl_config.ctx);
		pkeyutl_config.ctx = NULL;
	}

 end:
	free(passin);

	if (!pkeyutl_config.ctx) {
		BIO_puts(bio_err, "Error initializing context\n");
		ERR_print_errors(bio_err);
		return (1);
	}

	return (0);
}

static int
setup_peer(char *file)
{
	EVP_PKEY *peer = NULL;
	int ret;

	if (!pkeyutl_config.ctx) {
		BIO_puts(bio_err, "-peerkey command before -inkey\n");
		return (1);
	}
	peer = load_pubkey(bio_err, file, pkeyutl_config.peerform, 0, NULL,
	    "Peer Key");

	if (!peer) {
		BIO_printf(bio_err, "Error reading peer key %s\n", file);
		ERR_print_errors(bio_err);
		return (1);
	}
	ret = EVP_PKEY_derive_set_peer(pkeyutl_config.ctx, peer);

	EVP_PKEY_free(peer);
	if (ret <= 0) {
		ERR_print_errors(bio_err);
		return (1);
	}
	
	return (0);
}

static int
pkeyutl_pkeyopt(char *pkeyopt)
{
	if (!pkeyutl_config.ctx) {
		BIO_puts(bio_err, "-pkeyopt command before -inkey\n");
		return (1);
	} else if (pkey_ctrl_string(pkeyutl_config.ctx, pkeyopt) <= 0) {
		BIO_puts(bio_err, "parameter setting error\n");
		ERR_print_errors(bio_err);
		return (1);
	}

	return (0);
}

static int
do_keyop(EVP_PKEY_CTX * ctx, int pkey_op,
    unsigned char *out, size_t * poutlen,
    unsigned char *in, size_t inlen)
{
	int rv = 0;
	switch (pkey_op) {
	case EVP_PKEY_OP_VERIFYRECOVER:
		rv = EVP_PKEY_verify_recover(ctx, out, poutlen, in, inlen);
		break;

	case EVP_PKEY_OP_SIGN:
		rv = EVP_PKEY_sign(ctx, out, poutlen, in, inlen);
		break;

	case EVP_PKEY_OP_ENCRYPT:
		rv = EVP_PKEY_encrypt(ctx, out, poutlen, in, inlen);
		break;

	case EVP_PKEY_OP_DECRYPT:
		rv = EVP_PKEY_decrypt(ctx, out, poutlen, in, inlen);
		break;

	case EVP_PKEY_OP_DERIVE:
		rv = EVP_PKEY_derive(ctx, out, poutlen);
		break;

	}
	return rv;
}
