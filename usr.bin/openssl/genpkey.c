/* $OpenBSD: genpkey.c,v 1.17 2023/07/23 11:39:29 tb Exp $ */
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

static int init_keygen_file(BIO * err, EVP_PKEY_CTX **pctx, const char *file);
static int genpkey_cb(EVP_PKEY_CTX * ctx);

static struct {
	const EVP_CIPHER *cipher;
	EVP_PKEY_CTX **ctx;
	int do_param;
	char *outfile;
	int outformat;
	char *passarg;
	int text;
} cfg;

static int
genpkey_opt_algorithm(char *arg)
{
	if (!init_gen_str(bio_err, cfg.ctx, arg,
	    cfg.do_param))
		return (1);

	return (0);
}

static int
genpkey_opt_cipher(int argc, char **argv, int *argsused)
{
	char *name = argv[0];

	if (*name++ != '-')
		return (1);

	if (cfg.do_param == 1)
		return (1);

	if (strcmp(name, "none") == 0) {
		cfg.cipher = NULL;
		*argsused = 1;
		return (0);
	}

	if ((cfg.cipher = EVP_get_cipherbyname(name)) != NULL) {
		*argsused = 1;
		return (0);
	}

	return (1);
}

static int
genpkey_opt_paramfile(char *arg)
{
	if (cfg.do_param == 1)
		return (1);
	if (!init_keygen_file(bio_err, cfg.ctx, arg))
		return (1);

	return (0);
}

static int
genpkey_opt_pkeyopt(char *arg)
{
	if (*cfg.ctx == NULL) {
		BIO_puts(bio_err, "No keytype specified\n");
		return (1);
	}

	if (pkey_ctrl_string(*cfg.ctx, arg) <= 0) {
		BIO_puts(bio_err, "parameter setting error\n");
		ERR_print_errors(bio_err);
		return (1);
	}

	return (0);
}

static const struct option genpkey_options[] = {
	{
		.name = "algorithm",
		.argname = "name",
		.desc = "Public key algorithm to use (must precede -pkeyopt)",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = genpkey_opt_algorithm,
	},
	{
		.name = "genparam",
		.desc = "Generate a set of parameters instead of a private key",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.do_param,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output file to write to (default stdout)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.outfile,
	},
	{
		.name = "outform",
		.argname = "format",
		.desc = "Output format (DER or PEM)",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.outformat,
	},
	{
		.name = "paramfile",
		.argname = "file",
		.desc = "File to load public key algorithm parameters from\n"
		    "(must precede -pkeyopt)",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = genpkey_opt_paramfile,
	},
	{
		.name = "pass",
		.argname = "arg",
		.desc = "Output file password source",
		.type = OPTION_ARG,
		.opt.arg = &cfg.passarg,
	},
	{
		.name = "pkeyopt",
		.argname = "opt:value",
		.desc = "Set public key algorithm option to the given value",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = genpkey_opt_pkeyopt,
	},
	{
		.name = "text",
		.desc = "Print the private/public key in human readable form",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.text,
	},
	{
		.name = NULL,
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = genpkey_opt_cipher,
	},
	{NULL},
};

static void
genpkey_usage(void)
{
	fprintf(stderr,
	    "usage: genpkey [-algorithm alg] [cipher] [-genparam] [-out file]\n"
	    "    [-outform der | pem] [-paramfile file] [-pass arg]\n"
	    "    [-pkeyopt opt:value] [-text]\n\n");
	options_usage(genpkey_options);
}

int
genpkey_main(int argc, char **argv)
{
	BIO *in = NULL, *out = NULL;
	EVP_PKEY_CTX *ctx = NULL;
	EVP_PKEY *pkey = NULL;
	char *pass = NULL;
	int ret = 1, rv;

	if (pledge("stdio cpath wpath rpath tty", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.ctx = &ctx;
	cfg.outformat = FORMAT_PEM;

	if (options_parse(argc, argv, genpkey_options, NULL, NULL) != 0) {
		genpkey_usage();
		goto end;
	}

	if (ctx == NULL) {
		genpkey_usage();
		goto end;
	}

	if (!app_passwd(bio_err, cfg.passarg, NULL, &pass, NULL)) {
		BIO_puts(bio_err, "Error getting password\n");
		goto end;
	}
	if (cfg.outfile != NULL) {
		if ((out = BIO_new_file(cfg.outfile, "wb")) ==
		    NULL) {
			BIO_printf(bio_err, "Can't open output file %s\n",
			    cfg.outfile);
			goto end;
		}
	} else {
		out = BIO_new_fp(stdout, BIO_NOCLOSE);
	}

	EVP_PKEY_CTX_set_cb(ctx, genpkey_cb);
	EVP_PKEY_CTX_set_app_data(ctx, bio_err);

	if (cfg.do_param) {
		if (EVP_PKEY_paramgen(ctx, &pkey) <= 0) {
			BIO_puts(bio_err, "Error generating parameters\n");
			ERR_print_errors(bio_err);
			goto end;
		}
	} else {
		if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
			BIO_puts(bio_err, "Error generating key\n");
			ERR_print_errors(bio_err);
			goto end;
		}
	}

	if (cfg.do_param)
		rv = PEM_write_bio_Parameters(out, pkey);
	else if (cfg.outformat == FORMAT_PEM)
		rv = PEM_write_bio_PrivateKey(out, pkey, cfg.cipher,
		    NULL, 0, NULL, pass);
	else if (cfg.outformat == FORMAT_ASN1)
		rv = i2d_PrivateKey_bio(out, pkey);
	else {
		BIO_printf(bio_err, "Bad format specified for key\n");
		goto end;
	}

	if (rv <= 0) {
		BIO_puts(bio_err, "Error writing key\n");
		ERR_print_errors(bio_err);
	}
	if (cfg.text) {
		if (cfg.do_param)
			rv = EVP_PKEY_print_params(out, pkey, 0, NULL);
		else
			rv = EVP_PKEY_print_private(out, pkey, 0, NULL);

		if (rv <= 0) {
			BIO_puts(bio_err, "Error printing key\n");
			ERR_print_errors(bio_err);
		}
	}
	ret = 0;

 end:
	EVP_PKEY_free(pkey);
	EVP_PKEY_CTX_free(ctx);
	BIO_free_all(out);
	BIO_free(in);
	free(pass);

	return ret;
}

static int
init_keygen_file(BIO * err, EVP_PKEY_CTX ** pctx, const char *file)
{
	BIO *pbio;
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *ctx = NULL;
	if (*pctx) {
		BIO_puts(err, "Parameters already set!\n");
		return 0;
	}
	pbio = BIO_new_file(file, "r");
	if (!pbio) {
		BIO_printf(err, "Can't open parameter file %s\n", file);
		return 0;
	}
	pkey = PEM_read_bio_Parameters(pbio, NULL);
	BIO_free(pbio);

	if (!pkey) {
		BIO_printf(bio_err, "Error reading parameter file %s\n", file);
		return 0;
	}
	ctx = EVP_PKEY_CTX_new(pkey, NULL);
	if (!ctx)
		goto err;
	if (EVP_PKEY_keygen_init(ctx) <= 0)
		goto err;
	EVP_PKEY_free(pkey);
	*pctx = ctx;
	return 1;

 err:
	BIO_puts(err, "Error initializing context\n");
	ERR_print_errors(err);
	EVP_PKEY_CTX_free(ctx);
	EVP_PKEY_free(pkey);
	return 0;

}

int
init_gen_str(BIO * err, EVP_PKEY_CTX ** pctx, const char *algname, int do_param)
{
	const EVP_PKEY_ASN1_METHOD *ameth;
	EVP_PKEY_CTX *ctx = NULL;
	int pkey_id;

	if (*pctx) {
		BIO_puts(err, "Algorithm already set!\n");
		return 0;
	}
	ameth = EVP_PKEY_asn1_find_str(NULL, algname, -1);

	if (!ameth) {
		BIO_printf(bio_err, "Algorithm %s not found\n", algname);
		return 0;
	}
	ERR_clear_error();

	EVP_PKEY_asn1_get0_info(&pkey_id, NULL, NULL, NULL, NULL, ameth);
	ctx = EVP_PKEY_CTX_new_id(pkey_id, NULL);

	if (!ctx)
		goto err;
	if (do_param) {
		if (EVP_PKEY_paramgen_init(ctx) <= 0)
			goto err;
	} else {
		if (EVP_PKEY_keygen_init(ctx) <= 0)
			goto err;
	}

	*pctx = ctx;
	return 1;

 err:
	BIO_printf(err, "Error initializing %s context\n", algname);
	ERR_print_errors(err);
	EVP_PKEY_CTX_free(ctx);
	return 0;

}

static int
genpkey_cb(EVP_PKEY_CTX * ctx)
{
	char c = '*';
	BIO *b = EVP_PKEY_CTX_get_app_data(ctx);
	int p;
	p = EVP_PKEY_CTX_get_keygen_info(ctx, 0);
	if (p == 0)
		c = '.';
	if (p == 1)
		c = '+';
	if (p == 2)
		c = '*';
	if (p == 3)
		c = '\n';
	BIO_write(b, &c, 1);
	(void) BIO_flush(b);
	return 1;
}
