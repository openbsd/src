/* $OpenBSD: verify.c,v 1.14 2021/02/15 17:57:58 jsing Exp $ */
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
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

static int cb(int ok, X509_STORE_CTX *ctx);
static int check(X509_STORE *ctx, char *file, STACK_OF(X509) *uchain,
    STACK_OF(X509) *tchain, STACK_OF(X509_CRL) *crls);
static int vflags = 0;

static struct {
	char *CAfile;
	char *CApath;
	char *crlfile;
	char *trustfile;
	char *untfile;
	int verbose;
	X509_VERIFY_PARAM *vpm;
} verify_config;

static int
verify_opt_args(int argc, char **argv, int *argsused)
{
	int oargc = argc;
	int badarg = 0;

	if (!args_verify(&argv, &argc, &badarg, bio_err, &verify_config.vpm))
		return (1);
	if (badarg)
		return (1);

	*argsused = oargc - argc;

	return (0);
}

static const struct option verify_options[] = {
	{
		.name = "CAfile",
		.argname = "file",
		.desc = "Certificate Authority file",
		.type = OPTION_ARG,
		.opt.arg = &verify_config.CAfile,
	},
	{
		.name = "CApath",
		.argname = "path",
		.desc = "Certificate Authority path",
		.type = OPTION_ARG,
		.opt.arg = &verify_config.CApath,
	},
	{
		.name = "CRLfile",
		.argname = "file",
		.desc = "Certificate Revocation List file",
		.type = OPTION_ARG,
		.opt.arg = &verify_config.crlfile,
	},
	{
		.name = "trusted",
		.argname = "file",
		.desc = "Trusted certificates file",
		.type = OPTION_ARG,
		.opt.arg = &verify_config.trustfile,
	},
	{
		.name = "untrusted",
		.argname = "file",
		.desc = "Untrusted certificates file",
		.type = OPTION_ARG,
		.opt.arg = &verify_config.untfile,
	},
	{
		.name = "verbose",
		.desc = "Verbose",
		.type = OPTION_FLAG,
		.opt.flag = &verify_config.verbose,
	},
	{
		.name = NULL,
		.desc = "",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = verify_opt_args,
	},
	{ NULL },
};

static const struct option verify_shared_options[] = {
	{
		.name = "attime",
		.argname = "epoch",
		.desc = "Use epoch as the verification time",
	},
	{
		.name = "check_ss_sig",
		.desc = "Check the root CA self-signed certificate signature",
	},
	{
		.name = "crl_check",
		.desc = "Enable CRL checking for the leaf certificate",
	},
	{
		.name = "crl_check_all",
		.desc = "Enable CRL checking for the entire certificate chain",
	},
	{
		.name = "explicit_policy",
		.desc = "Require explicit policy (per RFC 3280)",
	},
	{
		.name = "extended_crl",
		.desc = "Enable extended CRL support",
	},
	{
		.name = "ignore_critical",
		.desc = "Disable critical extension checking",
	},
	{
		.name = "inhibit_any",
		.desc = "Inhibit any policy (per RFC 3280)",
	},
	{
		.name = "inhibit_map",
		.desc = "Inhibit policy mapping (per RFC 3280)",
	},
	{
		.name = "issuer_checks",
		.desc = "Enable debugging of certificate issuer checks",
	},
	{
		.name = "legacy_verify",
		.desc = "Use legacy certificate chain verification",
	},
	{
		.name = "policy",
		.argname = "name",
		.desc = "Add given policy to the acceptable set",
	},
	{
		.name = "policy_check",
		.desc = "Enable certificate policy checking",
	},
	{
		.name = "policy_print",
		.desc = "Print policy",
	},
	{
		.name = "purpose",
		.argname = "name",
		.desc = "Verify for the given purpose",
	},
	{
		.name = "use_deltas",
		.desc = "Use delta CRLS (if present)",
	},
	{
		.name = "verify_depth",
		.argname = "num",
		.desc = "Limit verification to the given depth",
	},
	{
		.name = "x509_strict",
		.desc = "Use strict X.509 rules (disables workarounds)",
	},
	{ NULL },
};

static void
verify_usage(void)
{
	int i;

	fprintf(stderr,
	    "usage: verify [-CAfile file] [-CApath directory] [-check_ss_sig]\n"
	    "    [-CRLfile file] [-crl_check] [-crl_check_all]\n"
	    "    [-explicit_policy] [-extended_crl]\n"
	    "    [-ignore_critical] [-inhibit_any] [-inhibit_map]\n"
	    "    [-issuer_checks] [-policy_check] [-purpose purpose]\n"
	    "    [-trusted file] [-untrusted file] [-verbose]\n"
	    "    [-x509_strict] [certificates]\n\n");

	options_usage(verify_options);

	fprintf(stderr, "\nVerification options:\n\n");
	options_usage(verify_shared_options);

	fprintf(stderr, "\nValid purposes:\n\n");
	for (i = 0; i < X509_PURPOSE_get_count(); i++) {
		X509_PURPOSE *ptmp = X509_PURPOSE_get0(i);
		fprintf(stderr, "  %-18s%s\n", X509_PURPOSE_get0_sname(ptmp),
		    X509_PURPOSE_get0_name(ptmp));
	}
}

int
verify_main(int argc, char **argv)
{
	STACK_OF(X509) *untrusted = NULL, *trusted = NULL;
	STACK_OF(X509_CRL) *crls = NULL;
	X509_STORE *cert_ctx = NULL;
	X509_LOOKUP *lookup = NULL;
	char **cert_files = NULL;
	int argsused;
	int ret = 1;

	if (single_execution) {
		if (pledge("stdio rpath", NULL) == -1) {
			perror("pledge");
			exit(1);
		}
	}

	memset(&verify_config, 0, sizeof(verify_config));

	if (options_parse(argc, argv, verify_options, NULL, &argsused) != 0) {
		verify_usage();
		goto end;
	}

	if (argsused < argc)
		cert_files = &argv[argsused];

	cert_ctx = X509_STORE_new();
	if (cert_ctx == NULL)
		goto end;
	X509_STORE_set_verify_cb(cert_ctx, cb);

	if (verify_config.vpm)
		X509_STORE_set1_param(cert_ctx, verify_config.vpm);

	lookup = X509_STORE_add_lookup(cert_ctx, X509_LOOKUP_file());
	if (lookup == NULL)
		abort(); /* XXX */
	if (verify_config.CAfile) {
		if (!X509_LOOKUP_load_file(lookup, verify_config.CAfile,
		    X509_FILETYPE_PEM)) {
			BIO_printf(bio_err, "Error loading file %s\n",
			    verify_config.CAfile);
			ERR_print_errors(bio_err);
			goto end;
		}
	} else
		X509_LOOKUP_load_file(lookup, NULL, X509_FILETYPE_DEFAULT);

	lookup = X509_STORE_add_lookup(cert_ctx, X509_LOOKUP_hash_dir());
	if (lookup == NULL)
		abort(); /* XXX */
	if (verify_config.CApath) {
		if (!X509_LOOKUP_add_dir(lookup, verify_config.CApath,
		    X509_FILETYPE_PEM)) {
			BIO_printf(bio_err, "Error loading directory %s\n",
			    verify_config.CApath);
			ERR_print_errors(bio_err);
			goto end;
		}
	} else
		X509_LOOKUP_add_dir(lookup, NULL, X509_FILETYPE_DEFAULT);

	ERR_clear_error();

	if (verify_config.untfile) {
		untrusted = load_certs(bio_err, verify_config.untfile,
		    FORMAT_PEM, NULL, "untrusted certificates");
		if (!untrusted)
			goto end;
	}
	if (verify_config.trustfile) {
		trusted = load_certs(bio_err, verify_config.trustfile,
		    FORMAT_PEM, NULL, "trusted certificates");
		if (!trusted)
			goto end;
	}
	if (verify_config.crlfile) {
		crls = load_crls(bio_err, verify_config.crlfile, FORMAT_PEM,
		    NULL, "other CRLs");
		if (!crls)
			goto end;
	}
	ret = 0;
	if (cert_files == NULL) {
		if (1 != check(cert_ctx, NULL, untrusted, trusted, crls))
			ret = -1;
	} else {
		do {
			if (1 != check(cert_ctx, *cert_files++, untrusted,
			    trusted, crls))
				ret = -1;
		} while (*cert_files != NULL);
	}

 end:
	if (verify_config.vpm)
		X509_VERIFY_PARAM_free(verify_config.vpm);
	if (cert_ctx != NULL)
		X509_STORE_free(cert_ctx);
	sk_X509_pop_free(untrusted, X509_free);
	sk_X509_pop_free(trusted, X509_free);
	sk_X509_CRL_pop_free(crls, X509_CRL_free);

	return (ret < 0 ? 2 : ret);
}

static int
check(X509_STORE *ctx, char *file, STACK_OF(X509) *uchain,
    STACK_OF(X509) *tchain, STACK_OF(X509_CRL) *crls)
{
	X509 *x = NULL;
	X509_STORE_CTX *csc = NULL;
	const char *certfile = (file == NULL) ? "stdin" : file;
	int verify_err;
	int i = 0, ret = 0;

	x = load_cert(bio_err, file, FORMAT_PEM, NULL, "certificate file");
	if (x == NULL)
		goto end;

	if ((csc = X509_STORE_CTX_new()) == NULL)
		goto end;
	X509_STORE_set_flags(ctx, vflags);
	if (!X509_STORE_CTX_init(csc, ctx, x, uchain))
		goto end;
	if (tchain)
		X509_STORE_CTX_trusted_stack(csc, tchain);
	if (crls)
		X509_STORE_CTX_set0_crls(csc, crls);

	i = X509_verify_cert(csc);
	verify_err = X509_STORE_CTX_get_error(csc);

	if (i > 0 && verify_err == X509_V_OK) {
		fprintf(stdout, "%s: OK\n", certfile);
		ret = 1;
	} else {
		fprintf(stdout, "%s: verification failed: %d (%s)\n", certfile,
		    verify_err, X509_verify_cert_error_string(verify_err));
	}

 end:
	if (i <= 0)
		ERR_print_errors(bio_err);
	X509_free(x);
	X509_STORE_CTX_free(csc);

	return (ret);
}

static int
cb(int ok, X509_STORE_CTX *ctx)
{
	int cert_error = X509_STORE_CTX_get_error(ctx);
	X509 *current_cert = X509_STORE_CTX_get_current_cert(ctx);

	if (!ok) {
		if (current_cert) {
			X509_NAME_print_ex_fp(stdout,
			    X509_get_subject_name(current_cert),
			    0, XN_FLAG_ONELINE);
			printf("\n");
		}
		printf("%serror %d at %d depth lookup:%s\n",
		    X509_STORE_CTX_get0_parent_ctx(ctx) ? "[CRL path]" : "",
		    cert_error,
		    X509_STORE_CTX_get_error_depth(ctx),
		    X509_verify_cert_error_string(cert_error));
		switch (cert_error) {
		case X509_V_ERR_NO_EXPLICIT_POLICY:
			policies_print(NULL, ctx);
		case X509_V_ERR_CERT_HAS_EXPIRED:

			/*
			 * since we are just checking the certificates, it is
			 * ok if they are self signed. But we should still
			 * warn the user.
			 */

		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
			/* Continue after extension errors too */
		case X509_V_ERR_INVALID_CA:
		case X509_V_ERR_INVALID_NON_CA:
		case X509_V_ERR_PATH_LENGTH_EXCEEDED:
		case X509_V_ERR_INVALID_PURPOSE:
		case X509_V_ERR_CRL_HAS_EXPIRED:
		case X509_V_ERR_CRL_NOT_YET_VALID:
		case X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION:
			ok = 1;

		}

		return ok;

	}
	if (cert_error == X509_V_OK && ok == 2)
		policies_print(NULL, ctx);
	if (!verify_config.verbose)
		ERR_clear_error();
	return (ok);
}
