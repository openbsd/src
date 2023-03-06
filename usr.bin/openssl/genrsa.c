/* $OpenBSD: genrsa.c,v 1.22 2023/03/06 14:32:06 tb Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <openssl/opensslconf.h>

/* Until the key-gen callbacks are modified to use newer prototypes, we allow
 * deprecated functions for openssl-internal code */
#ifdef OPENSSL_NO_DEPRECATED
#undef OPENSSL_NO_DEPRECATED
#endif


#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <string.h>

#include "apps.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#define DEFBITS	2048

static int genrsa_cb(int p, int n, BN_GENCB *cb);

static struct {
	const EVP_CIPHER *enc;
	unsigned long f4;
	char *outfile;
	char *passargout;
} cfg;

static int
set_public_exponent(int argc, char **argv, int *argsused)
{
	char *option = argv[0];

	if (strcmp(option, "-3") == 0)
		cfg.f4 = 3;
	else if (strcmp(option, "-f4") == 0 || strcmp(option, "-F4") == 0)
		cfg.f4 = RSA_F4;
	else
		return (1);

	*argsused = 1;
	return (0);
}

static const EVP_CIPHER *get_cipher_by_name(char *name)
{
	if (name == NULL || strcmp(name, "") == 0)
		return (NULL);
#ifndef OPENSSL_NO_AES
	else if (strcmp(name, "aes128") == 0)
		return EVP_aes_128_cbc();
	else if (strcmp(name, "aes192") == 0)
		return EVP_aes_192_cbc();
	else if (strcmp(name, "aes256") == 0)
		return EVP_aes_256_cbc();
#endif
#ifndef OPENSSL_NO_CAMELLIA
	else if (strcmp(name, "camellia128") == 0)
		return EVP_camellia_128_cbc();
	else if (strcmp(name, "camellia192") == 0)
		return EVP_camellia_192_cbc();
	else if (strcmp(name, "camellia256") == 0)
		return EVP_camellia_256_cbc();
#endif
#ifndef OPENSSL_NO_DES
	else if (strcmp(name, "des") == 0)
		return EVP_des_cbc();
	else if (strcmp(name, "des3") == 0)
		return EVP_des_ede3_cbc();
#endif
#ifndef OPENSSL_NO_IDEA
	else if (strcmp(name, "idea") == 0)
		return EVP_idea_cbc();
#endif
	else
		return (NULL);
}

static int
set_enc(int argc, char **argv, int *argsused)
{
	char *name = argv[0];

	if (*name++ != '-')
		return (1);

	if ((cfg.enc = get_cipher_by_name(name)) == NULL)
		return (1);

	*argsused = 1;
	return (0);
}

static const struct option genrsa_options[] = {
	{
		.name = "3",
		.desc = "Use 3 for the E value",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = set_public_exponent,
	},
	{
		.name = "f4",
		.desc = "Use F4 (0x10001) for the E value",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = set_public_exponent,
	},
	{
		.name = "F4",
		.desc = "Use F4 (0x10001) for the E value",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = set_public_exponent,
	},
#ifndef OPENSSL_NO_AES
	{
		.name = "aes128",
		.desc = "Encrypt PEM output with CBC AES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = set_enc,
	},
	{
		.name = "aes192",
		.desc = "Encrypt PEM output with CBC AES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = set_enc,
	},
	{
		.name = "aes256",
		.desc = "Encrypt PEM output with CBC AES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = set_enc,
	},
#endif
#ifndef OPENSSL_NO_CAMELLIA
	{
		.name = "camellia128",
		.desc = "Encrypt PEM output with CBC Camellia",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = set_enc,
	},
	{
		.name = "camellia192",
		.desc = "Encrypt PEM output with CBC Camellia",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = set_enc,
	},
	{
		.name = "camellia256",
		.desc = "Encrypt PEM output with CBC Camellia",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = set_enc,
	},
#endif
#ifndef OPENSSL_NO_DES
	{
		.name = "des",
		.desc = "Encrypt the generated key with DES in CBC mode",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = set_enc,
	},
	{
		.name = "des3",
		.desc = "Encrypt the generated key with DES in EDE CBC mode (168 bit key)",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = set_enc,
	},
#endif
#ifndef OPENSSL_NO_IDEA
	{
		.name = "idea",
		.desc = "Encrypt the generated key with IDEA in CBC mode",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = set_enc,
	},
#endif
	{
		.name = "out",
		.argname = "file",
		.desc = "Output the key to 'file'",
		.type = OPTION_ARG,
		.opt.arg = &cfg.outfile,
	},
	{
		.name = "passout",
		.argname = "arg",
		.desc = "Output file passphrase source",
		.type = OPTION_ARG,
		.opt.arg = &cfg.passargout,
	},
	{ NULL },
};

static void
genrsa_usage(void)
{
	fprintf(stderr, "usage: genrsa [-3 | -f4] [-aes128 | -aes192 |");
	fprintf(stderr, " -aes256 |\n");
	fprintf(stderr, "    -camellia128 | -camellia192 | -camellia256 |");
	fprintf(stderr, " -des | -des3 | -idea]\n");
	fprintf(stderr, "    [-out file] [-passout arg] [numbits]\n\n");
	options_usage(genrsa_options);
	fprintf(stderr, "\n");
}

int
genrsa_main(int argc, char **argv)
{
	BN_GENCB *cb = NULL;
	int ret = 1;
	int num = DEFBITS;
	char *numbits = NULL;
	char *passout = NULL;
	BIO *out = NULL;
	BIGNUM *bn = NULL;
	RSA *rsa = NULL;
	char *rsa_e_hex = NULL, *rsa_e_dec = NULL;

	if (pledge("stdio cpath wpath rpath tty", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	if ((bn = BN_new()) == NULL)
		goto err;

	if ((cb = BN_GENCB_new()) == NULL) {
		BIO_printf(bio_err, "Error allocating BN_GENCB object\n");
		goto err;
	}

	BN_GENCB_set(cb, genrsa_cb, bio_err);

	if ((out = BIO_new(BIO_s_file())) == NULL) {
		BIO_printf(bio_err, "unable to create BIO for output\n");
		goto err;
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.f4 = RSA_F4;

	if (options_parse(argc, argv, genrsa_options, &numbits, NULL) != 0) {
		genrsa_usage();
		goto err;
	}

	if ((numbits != NULL) &&
	    ((sscanf(numbits, "%d", &num) == 0) || (num < 0))) {
		genrsa_usage();
		goto err;
	}

	if (!app_passwd(bio_err, NULL, cfg.passargout, NULL,
	    &passout)) {
		BIO_printf(bio_err, "Error getting password\n");
		goto err;
	}

	if (cfg.outfile == NULL) {
		BIO_set_fp(out, stdout, BIO_NOCLOSE);
	} else {
		if (BIO_write_filename(out, cfg.outfile) <= 0) {
			perror(cfg.outfile);
			goto err;
		}
	}

	BIO_printf(bio_err, "Generating RSA private key, %d bit long modulus\n",
	    num);
	rsa = RSA_new();
	if (!rsa)
		goto err;

	if (!BN_set_word(bn, cfg.f4) ||
	    !RSA_generate_key_ex(rsa, num, bn, cb))
		goto err;

	if ((rsa_e_hex = BN_bn2hex(RSA_get0_e(rsa))) == NULL)
		goto err;
	if ((rsa_e_dec = BN_bn2dec(RSA_get0_e(rsa))) == NULL)
		goto err;

	BIO_printf(bio_err, "e is %s (0x%s)\n", rsa_e_dec, rsa_e_hex);
	{
		PW_CB_DATA cb_data;
		cb_data.password = passout;
		cb_data.prompt_info = cfg.outfile;
		if (!PEM_write_bio_RSAPrivateKey(out, rsa, cfg.enc,
		    NULL, 0, password_callback, &cb_data))
			goto err;
	}

	ret = 0;
 err:
	BN_free(bn);
	BN_GENCB_free(cb);
	RSA_free(rsa);
	BIO_free_all(out);
	free(rsa_e_dec);
	free(rsa_e_hex);
	free(passout);

	if (ret != 0)
		ERR_print_errors(bio_err);

	return (ret);
}

static int
genrsa_cb(int p, int n, BN_GENCB *cb)
{
	char c = '*';

	if (p == 0)
		c = '.';
	if (p == 1)
		c = '+';
	if (p == 2)
		c = '*';
	if (p == 3)
		c = '\n';
	BIO_write(BN_GENCB_get_arg(cb), &c, 1);
	(void) BIO_flush(BN_GENCB_get_arg(cb));
	return 1;
}
