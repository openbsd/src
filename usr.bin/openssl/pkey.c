/* $OpenBSD: pkey.c,v 1.15 2019/07/14 03:30:46 guenther Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006
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

#include <stdio.h>
#include <string.h>

#include "apps.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

static struct {
	const EVP_CIPHER *cipher;
	char *infile;
	int informat;
	int noout;
	char *outfile;
	int outformat;
	char *passargin;
	char *passargout;
	int pubin;
	int pubout;
	int pubtext;
	int text;
} pkey_config;

static int
pkey_opt_cipher(int argc, char **argv, int *argsused)
{
	char *name = argv[0];

	if (*name++ != '-')
		return (1);

	if ((pkey_config.cipher = EVP_get_cipherbyname(name)) == NULL) {
		BIO_printf(bio_err, "Unknown cipher %s\n", name);
		return (1);
	}

	*argsused = 1;
	return (0);
}

static const struct option pkey_options[] = {
	{
		.name = "in",
		.argname = "file",
		.desc = "Input file (default stdin)",
		.type = OPTION_ARG,
		.opt.arg = &pkey_config.infile,
	},
	{
		.name = "inform",
		.argname = "format",
		.desc = "Input format (DER or PEM (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &pkey_config.informat,
	},
	{
		.name = "noout",
		.desc = "Do not print encoded version of the key",
		.type = OPTION_FLAG,
		.opt.flag = &pkey_config.noout,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output file (default stdout)",
		.type = OPTION_ARG,
		.opt.arg = &pkey_config.outfile,
	},
	{
		.name = "outform",
		.argname = "format",
		.desc = "Output format (DER or PEM (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &pkey_config.outformat,
	},
	{
		.name = "passin",
		.argname = "src",
		.desc = "Input file passphrase source",
		.type = OPTION_ARG,
		.opt.arg = &pkey_config.passargin,
	},
	{
		.name = "passout",
		.argname = "src",
		.desc = "Output file passphrase source",
		.type = OPTION_ARG,
		.opt.arg = &pkey_config.passargout,
	},
	{
		.name = "pubin",
		.desc = "Expect a public key (default private key)",
		.type = OPTION_VALUE,
		.value = 1,
		.opt.value = &pkey_config.pubin,
	},
	{
		.name = "pubout",
		.desc = "Output a public key (default private key)",
		.type = OPTION_VALUE,
		.value = 1,
		.opt.value = &pkey_config.pubout,
	},
	{
		.name = "text",
		.desc = "Print the public/private key in plain text",
		.type = OPTION_FLAG,
		.opt.flag = &pkey_config.text,
	},
	{
		.name = "text_pub",
		.desc = "Print out only public key in plain text",
		.type = OPTION_FLAG,
		.opt.flag = &pkey_config.pubtext,
	},
	{
		.name = NULL,
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = pkey_opt_cipher,
	},
	{ NULL }
};

static void
pkey_usage()
{
	int n = 0;

	fprintf(stderr,
	    "usage: pkey [-ciphername] [-in file] [-inform fmt] [-noout] "
	    "[-out file]\n"
	    "    [-outform fmt] [-passin src] [-passout src] [-pubin] "
	    "[-pubout] [-text]\n"
	    "    [-text_pub]\n\n");
	options_usage(pkey_options);
	fprintf(stderr, "\n");

	fprintf(stderr, "Valid ciphername values:\n\n");
	OBJ_NAME_do_all_sorted(OBJ_NAME_TYPE_CIPHER_METH, show_cipher, &n);
	fprintf(stderr, "\n");
}

int
pkey_main(int argc, char **argv)
{
	BIO *in = NULL, *out = NULL;
	EVP_PKEY *pkey = NULL;
	char *passin = NULL, *passout = NULL;
	int ret = 1;

	if (single_execution) {
		if (pledge("stdio cpath wpath rpath tty", NULL) == -1) {
			perror("pledge");
			exit(1);
		}
	}

	memset(&pkey_config, 0, sizeof(pkey_config));
	pkey_config.informat = FORMAT_PEM;
	pkey_config.outformat = FORMAT_PEM;

	if (options_parse(argc, argv, pkey_options, NULL, NULL) != 0) {
		pkey_usage();
		goto end;
	}

	if (pkey_config.pubtext)
		pkey_config.text = 1;
	if (pkey_config.pubin)
		pkey_config.pubout = pkey_config.pubtext = 1;

	if (!app_passwd(bio_err, pkey_config.passargin, pkey_config.passargout,
	    &passin, &passout)) {
		BIO_printf(bio_err, "Error getting passwords\n");
		goto end;
	}
	if (pkey_config.outfile) {
		if (!(out = BIO_new_file(pkey_config.outfile, "wb"))) {
			BIO_printf(bio_err,
			    "Can't open output file %s\n", pkey_config.outfile);
			goto end;
		}
	} else {
		out = BIO_new_fp(stdout, BIO_NOCLOSE);
	}

	if (pkey_config.pubin)
		pkey = load_pubkey(bio_err, pkey_config.infile,
		    pkey_config.informat, 1, passin, "Public Key");
	else
		pkey = load_key(bio_err, pkey_config.infile,
		    pkey_config.informat, 1, passin, "key");
	if (!pkey)
		goto end;

	if (!pkey_config.noout) {
		if (pkey_config.outformat == FORMAT_PEM) {
			if (pkey_config.pubout)
				PEM_write_bio_PUBKEY(out, pkey);
			else
				PEM_write_bio_PrivateKey(out, pkey,
				    pkey_config.cipher, NULL, 0, NULL, passout);
		} else if (pkey_config.outformat == FORMAT_ASN1) {
			if (pkey_config.pubout)
				i2d_PUBKEY_bio(out, pkey);
			else
				i2d_PrivateKey_bio(out, pkey);
		} else {
			BIO_printf(bio_err, "Bad format specified for key\n");
			goto end;
		}

	}
	if (pkey_config.text) {
		if (pkey_config.pubtext)
			EVP_PKEY_print_public(out, pkey, 0, NULL);
		else
			EVP_PKEY_print_private(out, pkey, 0, NULL);
	}
	ret = 0;

 end:
	EVP_PKEY_free(pkey);
	BIO_free_all(out);
	BIO_free(in);
	free(passin);
	free(passout);

	return ret;
}
