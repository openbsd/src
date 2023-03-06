/* $OpenBSD: dsaparam.c,v 1.15 2023/03/06 14:32:06 tb Exp $ */
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

#include <openssl/opensslconf.h>	/* for OPENSSL_NO_DSA */

/* Until the key-gen callbacks are modified to use newer prototypes, we allow
 * deprecated functions for openssl-internal code */
#ifdef OPENSSL_NO_DEPRECATED
#undef OPENSSL_NO_DEPRECATED
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "apps.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/dsa.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

static struct {
	int C;
	int genkey;
	char *infile;
	int informat;
	int noout;
	char *outfile;
	int outformat;
	int text;
} cfg;

static const struct option dsaparam_options[] = {
	{
		.name = "C",
		.desc = "Convert DSA parameters into C code",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.C,
	},
	{
		.name = "genkey",
		.desc = "Generate a DSA key",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.genkey,
	},
	{
		.name = "in",
		.argname = "file",
		.desc = "Input file (default stdin)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.infile,
	},
	{
		.name = "inform",
		.argname = "format",
		.desc = "Input format (DER or PEM (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.informat,
	},
	{
		.name = "noout",
		.desc = "No output",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.noout,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output file (default stdout)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.outfile,
	},
	{
		.name = "outform",
		.argname = "format",
		.desc = "Output format (DER or PEM (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.outformat,
	},
	{
		.name = "text",
		.desc = "Print as text",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.text,
	},
	{ NULL },
};

static void
dsaparam_usage(void)
{
	fprintf(stderr,
	    "usage: dsaparam [-C] [-genkey] [-in file]\n"
	    "    [-inform format] [-noout] [-out file] [-outform format]\n"
	    "    [-text] [numbits]\n\n");
	options_usage(dsaparam_options);
}

static int dsa_cb(int p, int n, BN_GENCB *cb);

int
dsaparam_main(int argc, char **argv)
{
	DSA *dsa = NULL;
	int i;
	BIO *in = NULL, *out = NULL;
	BN_GENCB *cb = NULL;
	int ret = 1;
	int numbits = -1;
	char *strbits = NULL;

	if (pledge("stdio cpath wpath rpath", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	memset(&cfg, 0, sizeof(cfg));

	cfg.informat = FORMAT_PEM;
	cfg.outformat = FORMAT_PEM;

	if (options_parse(argc, argv, dsaparam_options, &strbits, NULL) != 0) {
		dsaparam_usage();
		goto end;
	}

	if (strbits != NULL) {
		const char *errstr;
		numbits = strtonum(strbits, 0, INT_MAX, &errstr);
		if (errstr) {
			fprintf(stderr, "Invalid number of bits: %s", errstr);
			goto end;
		}
	}

	in = BIO_new(BIO_s_file());
	out = BIO_new(BIO_s_file());
	if (in == NULL || out == NULL) {
		ERR_print_errors(bio_err);
		goto end;
	}
	if (cfg.infile == NULL)
		BIO_set_fp(in, stdin, BIO_NOCLOSE);
	else {
		if (BIO_read_filename(in, cfg.infile) <= 0) {
			perror(cfg.infile);
			goto end;
		}
	}
	if (cfg.outfile == NULL) {
		BIO_set_fp(out, stdout, BIO_NOCLOSE);
	} else {
		if (BIO_write_filename(out, cfg.outfile) <= 0) {
			perror(cfg.outfile);
			goto end;
		}
	}

	if (numbits > 0) {
		if ((cb = BN_GENCB_new()) == NULL) {
			BIO_printf(bio_err,
			    "Error allocating BN_GENCB object\n");
			goto end;
		}

		BN_GENCB_set(cb, dsa_cb, bio_err);

		dsa = DSA_new();
		if (!dsa) {
			BIO_printf(bio_err, "Error allocating DSA object\n");
			goto end;
		}
		BIO_printf(bio_err, "Generating DSA parameters, %d bit long prime\n", numbits);
		BIO_printf(bio_err, "This could take some time\n");
		if (!DSA_generate_parameters_ex(dsa, numbits, NULL, 0, NULL, NULL, cb)) {
			ERR_print_errors(bio_err);
			BIO_printf(bio_err, "Error, DSA key generation failed\n");
			goto end;
		}
	} else if (cfg.informat == FORMAT_ASN1)
		dsa = d2i_DSAparams_bio(in, NULL);
	else if (cfg.informat == FORMAT_PEM)
		dsa = PEM_read_bio_DSAparams(in, NULL, NULL, NULL);
	else {
		BIO_printf(bio_err, "bad input format specified\n");
		goto end;
	}
	if (dsa == NULL) {
		BIO_printf(bio_err, "unable to load DSA parameters\n");
		ERR_print_errors(bio_err);
		goto end;
	}
	if (cfg.text) {
		DSAparams_print(out, dsa);
	}
	if (cfg.C) {
		unsigned char *data;
		int l, len, bits_p;

		len = BN_num_bytes(DSA_get0_p(dsa));
		bits_p = BN_num_bits(DSA_get0_p(dsa));
		data = malloc(len + 20);
		if (data == NULL) {
			perror("malloc");
			goto end;
		}
		l = BN_bn2bin(DSA_get0_p(dsa), data);
		printf("static unsigned char dsa%d_p[] = {", bits_p);
		for (i = 0; i < l; i++) {
			if ((i % 12) == 0)
				printf("\n\t");
			printf("0x%02X, ", data[i]);
		}
		printf("\n\t};\n");

		l = BN_bn2bin(DSA_get0_q(dsa), data);
		printf("static unsigned char dsa%d_q[] = {", bits_p);
		for (i = 0; i < l; i++) {
			if ((i % 12) == 0)
				printf("\n\t");
			printf("0x%02X, ", data[i]);
		}
		printf("\n\t};\n");

		l = BN_bn2bin(DSA_get0_g(dsa), data);
		printf("static unsigned char dsa%d_g[] = {", bits_p);
		for (i = 0; i < l; i++) {
			if ((i % 12) == 0)
				printf("\n\t");
			printf("0x%02X, ", data[i]);
		}
		free(data);
		printf("\n\t};\n\n");

		printf("DSA *get_dsa%d()\n\t{\n", bits_p);
		printf("\tBIGNUM *p = NULL, *q = NULL, *g = NULL;\n");
		printf("\tDSA *dsa;\n\n");
		printf("\tif ((dsa = DSA_new()) == NULL) return(NULL);\n");
		printf("\tp = BN_bin2bn(dsa%d_p, sizeof(dsa%d_p), NULL);\n",
		    bits_p, bits_p);
		printf("\tq = BN_bin2bn(dsa%d_q, sizeof(dsa%d_q), NULL);\n",
		    bits_p, bits_p);
		printf("\tg = BN_bin2bn(dsa%d_g, sizeof(dsa%d_g), NULL);\n",
		    bits_p, bits_p);
		printf("\tif (p == NULL || q == NULL || g == NULL)\n");
		printf("\t\t{ BN_free(p); BN_free(q); BN_free(g); DSA_free(dsa); return(NULL); }\n");
		printf("\tDSA_set0_pqg(dsa, p, q, g);\n");
		printf("\treturn(dsa);\n\t}\n");
	}
	if (!cfg.noout) {
		if (cfg.outformat == FORMAT_ASN1)
			i = i2d_DSAparams_bio(out, dsa);
		else if (cfg.outformat == FORMAT_PEM)
			i = PEM_write_bio_DSAparams(out, dsa);
		else {
			BIO_printf(bio_err, "bad output format specified for outfile\n");
			goto end;
		}
		if (!i) {
			BIO_printf(bio_err, "unable to write DSA parameters\n");
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	if (cfg.genkey) {
		DSA *dsakey;

		if ((dsakey = DSAparams_dup(dsa)) == NULL)
			goto end;
		if (!DSA_generate_key(dsakey)) {
			ERR_print_errors(bio_err);
			DSA_free(dsakey);
			goto end;
		}
		if (cfg.outformat == FORMAT_ASN1)
			i = i2d_DSAPrivateKey_bio(out, dsakey);
		else if (cfg.outformat == FORMAT_PEM)
			i = PEM_write_bio_DSAPrivateKey(out, dsakey, NULL, NULL, 0, NULL, NULL);
		else {
			BIO_printf(bio_err, "bad output format specified for outfile\n");
			DSA_free(dsakey);
			goto end;
		}
		DSA_free(dsakey);
	}
	ret = 0;

 end:
	BIO_free(in);
	BIO_free_all(out);
	BN_GENCB_free(cb);
	DSA_free(dsa);

	return (ret);
}

static int
dsa_cb(int p, int n, BN_GENCB *cb)
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
#ifdef GENCB_TEST
	if (stop_keygen_flag)
		return 0;
#endif
	return 1;
}
