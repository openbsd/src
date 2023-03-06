/* $OpenBSD: dgst.c,v 1.21 2023/03/06 14:32:05 tb Exp $ */
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apps.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#define BUFSIZE	1024*8

int
do_fp(BIO * out, unsigned char *buf, BIO * bp, int sep, int binout,
    EVP_PKEY * key, unsigned char *sigin, int siglen,
    const char *sig_name, const char *md_name,
    const char *file, BIO * bmd);

static struct {
	int argsused;
	int debug;
	int do_verify;
	char *hmac_key;
	char *keyfile;
	int keyform;
	const EVP_MD *m;
	char *mac_name;
	STACK_OF(OPENSSL_STRING) *macopts;
	const EVP_MD *md;
	int out_bin;
	char *outfile;
	char *passargin;
	int separator;
	char *sigfile;
	STACK_OF(OPENSSL_STRING) *sigopts;
	int want_pub;
} cfg;

static int
dgst_opt_macopt(char *arg)
{
	if (arg == NULL)
		return (1);

	if (cfg.macopts == NULL &&
	    (cfg.macopts = sk_OPENSSL_STRING_new_null()) == NULL)
		return (1);

	if (!sk_OPENSSL_STRING_push(cfg.macopts, arg))
		return (1);

	return (0);
}

static int
dgst_opt_md(int argc, char **argv, int *argsused)
{
	char *name = argv[0];

	if (*name++ != '-')
		return (1);

	if ((cfg.m = EVP_get_digestbyname(name)) == NULL)
		return (1);

	cfg.md = cfg.m;

	*argsused = 1;
	return (0);
}

static int
dgst_opt_prverify(char *arg)
{
	if (arg == NULL)
		return (1);

	cfg.keyfile = arg;
	cfg.do_verify = 1;
	return (0);
}

static int
dgst_opt_sigopt(char *arg)
{
	if (arg == NULL)
		return (1);

	if (cfg.sigopts == NULL &&
	    (cfg.sigopts = sk_OPENSSL_STRING_new_null()) == NULL)
		return (1);

	if (!sk_OPENSSL_STRING_push(cfg.sigopts, arg))
		return (1);

	return (0);
}

static int
dgst_opt_verify(char *arg)
{
	if (arg == NULL)
		return (1);

	cfg.keyfile = arg;
	cfg.want_pub = 1;
	cfg.do_verify = 1;
	return (0);
}

static const struct option dgst_options[] = {
	{
		.name = "binary",
		.desc = "Output the digest or signature in binary form",
		.type = OPTION_VALUE,
		.opt.value = &cfg.out_bin,
		.value = 1,
	},
	{
		.name = "c",
		.desc = "Print the digest in two-digit groups separated by colons",
		.type = OPTION_VALUE,
		.opt.value = &cfg.separator,
		.value = 1,
	},
	{
		.name = "d",
		.desc = "Print BIO debugging information",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.debug,
	},
	{
		.name = "hex",
		.desc = "Output as hex dump",
		.type = OPTION_VALUE,
		.opt.value = &cfg.out_bin,
		.value = 0,
	},
	{
		.name = "hmac",
		.argname = "key",
		.desc = "Create hashed MAC with key",
		.type = OPTION_ARG,
		.opt.arg = &cfg.hmac_key,
	},
	{
		.name = "keyform",
		.argname = "format",
		.desc = "Key file format (PEM)",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.keyform,
	},
	{
		.name = "mac",
		.argname = "algorithm",
		.desc = "Create MAC (not necessarily HMAC)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.mac_name,
	},
	{
		.name = "macopt",
		.argname = "nm:v",
		.desc = "MAC algorithm parameters or key",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = dgst_opt_macopt,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output to file rather than stdout",
		.type = OPTION_ARG,
		.opt.arg = &cfg.outfile,
	},
	{
		.name = "passin",
		.argname = "arg",
		.desc = "Input file passphrase source",
		.type = OPTION_ARG,
		.opt.arg = &cfg.passargin,
	},
	{
		.name = "prverify",
		.argname = "file",
		.desc = "Verify a signature using private key in file",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = dgst_opt_prverify,
	},
	{
		.name = "r",
		.desc = "Output the digest in coreutils format",
		.type = OPTION_VALUE,
		.opt.value = &cfg.separator,
		.value = 2,
	},
	{
		.name = "sign",
		.argname = "file",
		.desc = "Sign digest using private key in file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.keyfile,
	},
	{
		.name = "signature",
		.argname = "file",
		.desc = "Signature to verify",
		.type = OPTION_ARG,
		.opt.arg = &cfg.sigfile,
	},
	{
		.name = "sigopt",
		.argname = "nm:v",
		.desc = "Signature parameter",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = dgst_opt_sigopt,
	},
	{
		.name = "verify",
		.argname = "file",
		.desc = "Verify a signature using public key in file",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = dgst_opt_verify,
	},
	{
		.name = NULL,
		.desc = "",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = dgst_opt_md,
	},
	{ NULL },
};

static void
list_md_fn(const EVP_MD * m, const char *from, const char *to, void *arg)
{
	const char *mname;
	/* Skip aliases */
	if (!m)
		return;
	mname = OBJ_nid2ln(EVP_MD_type(m));
	/* Skip shortnames */
	if (strcmp(from, mname))
		return;
	if (strchr(mname, ' '))
		mname = EVP_MD_name(m);
	BIO_printf(arg, " -%-17s To use the %s message digest algorithm\n",
	    mname, mname);
}

static void
dgst_usage(void)
{
	fprintf(stderr, "usage: dgst [-cdr] [-binary] [-digest] [-hex]");
	fprintf(stderr, " [-hmac key] [-keyform fmt]\n");
	fprintf(stderr, "    [-mac algorithm] [-macopt nm:v] [-out file]");
	fprintf(stderr, " [-passin arg]\n");
	fprintf(stderr, "    [-prverify file] [-sign file]");
	fprintf(stderr, " [-signature file]\n");
	fprintf(stderr, "    [-sigopt nm:v] [-verify file] [file ...]\n\n");
	options_usage(dgst_options);
	EVP_MD_do_all_sorted(list_md_fn, bio_err);
	fprintf(stderr, "\n");
}

int
dgst_main(int argc, char **argv)
{
	unsigned char *buf = NULL;
	int i, err = 1;
	BIO *in = NULL, *inp;
	BIO *bmd = NULL;
	BIO *out = NULL;
#define PROG_NAME_SIZE  39
	char pname[PROG_NAME_SIZE + 1];
	EVP_PKEY *sigkey = NULL;
	unsigned char *sigbuf = NULL;
	int siglen = 0;
	char *passin = NULL;

	if (pledge("stdio cpath wpath rpath tty", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	if ((buf = malloc(BUFSIZE)) == NULL) {
		BIO_printf(bio_err, "out of memory\n");
		goto end;
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.keyform = FORMAT_PEM;
	cfg.out_bin = -1;

	/* first check the program name */
	program_name(argv[0], pname, sizeof pname);

	cfg.md = EVP_get_digestbyname(pname);

	if (options_parse(argc, argv, dgst_options, NULL,
	    &cfg.argsused) != 0) {
		dgst_usage();
		goto end;
	}
	argc -= cfg.argsused;
	argv += cfg.argsused;

	if (cfg.do_verify && !cfg.sigfile) {
		BIO_printf(bio_err,
		    "No signature to verify: use the -signature option\n");
		goto end;
	}

	in = BIO_new(BIO_s_file());
	bmd = BIO_new(BIO_f_md());
	if (in == NULL || bmd == NULL) {
		ERR_print_errors(bio_err);
		goto end;
	}

	if (cfg.debug) {
		BIO_set_callback(in, BIO_debug_callback);
		/* needed for windows 3.1 */
		BIO_set_callback_arg(in, (char *) bio_err);
	}
	if (!app_passwd(bio_err, cfg.passargin, NULL, &passin, NULL)) {
		BIO_printf(bio_err, "Error getting password\n");
		goto end;
	}
	if (cfg.out_bin == -1) {
		if (cfg.keyfile)
			cfg.out_bin = 1;
		else
			cfg.out_bin = 0;
	}

	if (cfg.outfile) {
		if (cfg.out_bin)
			out = BIO_new_file(cfg.outfile, "wb");
		else
			out = BIO_new_file(cfg.outfile, "w");
	} else {
		out = BIO_new_fp(stdout, BIO_NOCLOSE);
	}

	if (!out) {
		BIO_printf(bio_err, "Error opening output file %s\n",
		    cfg.outfile ? cfg.outfile : "(stdout)");
		ERR_print_errors(bio_err);
		goto end;
	}
	if ((!!cfg.mac_name + !!cfg.keyfile +
	    !!cfg.hmac_key) > 1) {
		BIO_printf(bio_err,
		    "MAC and Signing key cannot both be specified\n");
		goto end;
	}
	if (cfg.keyfile) {
		if (cfg.want_pub)
			sigkey = load_pubkey(bio_err, cfg.keyfile,
			    cfg.keyform, 0, NULL, "key file");
		else
			sigkey = load_key(bio_err, cfg.keyfile,
			    cfg.keyform, 0, passin, "key file");
		if (!sigkey) {
			/*
			 * load_[pub]key() has already printed an appropriate
			 * message
			 */
			goto end;
		}
	}
	if (cfg.mac_name) {
		EVP_PKEY_CTX *mac_ctx = NULL;
		int r = 0;
		if (!init_gen_str(bio_err, &mac_ctx, cfg.mac_name, 0))
			goto mac_end;
		if (cfg.macopts) {
			char *macopt;
			for (i = 0; i < sk_OPENSSL_STRING_num(
			    cfg.macopts); i++) {
				macopt = sk_OPENSSL_STRING_value(
				    cfg.macopts, i);
				if (pkey_ctrl_string(mac_ctx, macopt) <= 0) {
					BIO_printf(bio_err,
					    "MAC parameter error \"%s\"\n",
					    macopt);
					ERR_print_errors(bio_err);
					goto mac_end;
				}
			}
		}
		if (EVP_PKEY_keygen(mac_ctx, &sigkey) <= 0) {
			BIO_puts(bio_err, "Error generating key\n");
			ERR_print_errors(bio_err);
			goto mac_end;
		}
		r = 1;
 mac_end:
		EVP_PKEY_CTX_free(mac_ctx);
		if (r == 0)
			goto end;
	}
	if (cfg.hmac_key) {
		sigkey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL,
		    (unsigned char *) cfg.hmac_key, -1);
		if (!sigkey)
			goto end;
	}
	if (sigkey) {
		EVP_MD_CTX *mctx = NULL;
		EVP_PKEY_CTX *pctx = NULL;
		int r;
		if (!BIO_get_md_ctx(bmd, &mctx)) {
			BIO_printf(bio_err, "Error getting context\n");
			ERR_print_errors(bio_err);
			goto end;
		}
		if (cfg.do_verify)
			r = EVP_DigestVerifyInit(mctx, &pctx, cfg.md,
			    NULL, sigkey);
		else
			r = EVP_DigestSignInit(mctx, &pctx, cfg.md,
			    NULL, sigkey);
		if (!r) {
			BIO_printf(bio_err, "Error setting context\n");
			ERR_print_errors(bio_err);
			goto end;
		}
		if (cfg.sigopts) {
			char *sigopt;
			for (i = 0; i < sk_OPENSSL_STRING_num(
			    cfg.sigopts); i++) {
				sigopt = sk_OPENSSL_STRING_value(
				    cfg.sigopts, i);
				if (pkey_ctrl_string(pctx, sigopt) <= 0) {
					BIO_printf(bio_err,
					    "parameter error \"%s\"\n",
					    sigopt);
					ERR_print_errors(bio_err);
					goto end;
				}
			}
		}
	}
	/* we use md as a filter, reading from 'in' */
	else {
		if (cfg.md == NULL)
			cfg.md = EVP_sha256();
		if (!BIO_set_md(bmd, cfg.md)) {
			BIO_printf(bio_err, "Error setting digest %s\n", pname);
			ERR_print_errors(bio_err);
			goto end;
		}
	}

	if (cfg.sigfile && sigkey) {
		BIO *sigbio;
		siglen = EVP_PKEY_size(sigkey);
		sigbuf = malloc(siglen);
		if (sigbuf == NULL) {
			BIO_printf(bio_err, "out of memory\n");
			ERR_print_errors(bio_err);
			goto end;
		}
		sigbio = BIO_new_file(cfg.sigfile, "rb");
		if (!sigbio) {
			BIO_printf(bio_err, "Error opening signature file %s\n",
			    cfg.sigfile);
			ERR_print_errors(bio_err);
			goto end;
		}
		siglen = BIO_read(sigbio, sigbuf, siglen);
		BIO_free(sigbio);
		if (siglen <= 0) {
			BIO_printf(bio_err, "Error reading signature file %s\n",
			    cfg.sigfile);
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	inp = BIO_push(bmd, in);

	if (cfg.md == NULL) {
		EVP_MD_CTX *tctx;
		BIO_get_md_ctx(bmd, &tctx);
		cfg.md = EVP_MD_CTX_md(tctx);
	}
	if (argc == 0) {
		BIO_set_fp(in, stdin, BIO_NOCLOSE);
		err = do_fp(out, buf, inp, cfg.separator,
		    cfg.out_bin, sigkey, sigbuf, siglen, NULL, NULL,
		    "stdin", bmd);
	} else {
		const char *md_name = NULL, *sig_name = NULL;
		if (!cfg.out_bin) {
			if (sigkey) {
				const EVP_PKEY_ASN1_METHOD *ameth;
				ameth = EVP_PKEY_get0_asn1(sigkey);
				if (ameth)
					EVP_PKEY_asn1_get0_info(NULL, NULL,
					    NULL, NULL, &sig_name, ameth);
			}
			md_name = EVP_MD_name(cfg.md);
		}
		err = 0;
		for (i = 0; i < argc; i++) {
			int r;
			if (BIO_read_filename(in, argv[i]) <= 0) {
				perror(argv[i]);
				err++;
				continue;
			} else {
				r = do_fp(out, buf, inp, cfg.separator,
				    cfg.out_bin, sigkey, sigbuf, siglen,
				    sig_name, md_name, argv[i], bmd);
			}
			if (r)
				err = r;
			(void) BIO_reset(bmd);
		}
	}

 end:
	freezero(buf, BUFSIZE);
	BIO_free(in);
	free(passin);
	BIO_free_all(out);
	EVP_PKEY_free(sigkey);
	sk_OPENSSL_STRING_free(cfg.sigopts);
	sk_OPENSSL_STRING_free(cfg.macopts);
	free(sigbuf);
	BIO_free(bmd);

	return (err);
}

int
do_fp(BIO * out, unsigned char *buf, BIO * bp, int sep, int binout,
    EVP_PKEY * key, unsigned char *sigin, int siglen,
    const char *sig_name, const char *md_name,
    const char *file, BIO * bmd)
{
	size_t len;
	int i;

	for (;;) {
		i = BIO_read(bp, (char *) buf, BUFSIZE);
		if (i < 0) {
			BIO_printf(bio_err, "Read Error in %s\n", file);
			ERR_print_errors(bio_err);
			return 1;
		}
		if (i == 0)
			break;
	}
	if (sigin) {
		EVP_MD_CTX *ctx;
		BIO_get_md_ctx(bp, &ctx);
		i = EVP_DigestVerifyFinal(ctx, sigin, (unsigned int) siglen);
		if (i > 0)
			BIO_printf(out, "Verified OK\n");
		else if (i == 0) {
			BIO_printf(out, "Verification Failure\n");
			return 1;
		} else {
			BIO_printf(bio_err, "Error Verifying Data\n");
			ERR_print_errors(bio_err);
			return 1;
		}
		return 0;
	}
	if (key) {
		EVP_MD_CTX *ctx;
		BIO_get_md_ctx(bp, &ctx);
		len = BUFSIZE;
		if (!EVP_DigestSignFinal(ctx, buf, &len)) {
			BIO_printf(bio_err, "Error Signing Data\n");
			ERR_print_errors(bio_err);
			return 1;
		}
	} else {
		len = BIO_gets(bp, (char *) buf, BUFSIZE);
		if ((int) len < 0) {
			ERR_print_errors(bio_err);
			return 1;
		}
	}

	if (binout)
		BIO_write(out, buf, len);
	else if (sep == 2) {
		for (i = 0; i < (int) len; i++)
			BIO_printf(out, "%02x", buf[i]);
		BIO_printf(out, " *%s\n", file);
	} else {
		if (sig_name)
			BIO_printf(out, "%s-%s(%s)= ", sig_name, md_name, file);
		else if (md_name)
			BIO_printf(out, "%s(%s)= ", md_name, file);
		else
			BIO_printf(out, "(%s)= ", file);
		for (i = 0; i < (int) len; i++) {
			if (sep && (i != 0))
				BIO_printf(out, ":");
			BIO_printf(out, "%02x", buf[i]);
		}
		BIO_printf(out, "\n");
	}
	return 0;
}
