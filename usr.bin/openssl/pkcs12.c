/* $OpenBSD: pkcs12.c,v 1.14 2019/07/26 12:35:59 inoguchi Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 1999-2006 The OpenSSL Project.  All rights reserved.
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

#if !defined(OPENSSL_NO_DES) && !defined(OPENSSL_NO_SHA1)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apps.h"

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>

#define NOKEYS		0x1
#define NOCERTS 	0x2
#define INFO		0x4
#define CLCERTS		0x8
#define CACERTS		0x10

int get_cert_chain(X509 *cert, X509_STORE *store, STACK_OF(X509) **chain);
int dump_certs_keys_p12(BIO *out, PKCS12 *p12, char *pass, int passlen,
    int options, char *pempass);
int dump_certs_pkeys_bags(BIO *out, STACK_OF(PKCS12_SAFEBAG) *bags, char *pass,
    int passlen, int options, char *pempass);
int dump_certs_pkeys_bag(BIO *out, PKCS12_SAFEBAG *bags, char *pass,
    int passlen, int options, char *pempass);
int print_attribs(BIO *out, STACK_OF(X509_ATTRIBUTE) *attrlst,
    const char *name);
void hex_prin(BIO *out, unsigned char *buf, int len);
int alg_print(BIO *x, X509_ALGOR *alg);
int cert_load(BIO *in, STACK_OF(X509) *sk);
static int set_pbe(BIO *err, int *ppbe, const char *str);

static struct {
	int add_lmk;
	char *CAfile;
	STACK_OF(OPENSSL_STRING) *canames;
	char *CApath;
	int cert_pbe;
	char *certfile;
	int chain;
	char *csp_name;
	const EVP_CIPHER *enc;
	int export_cert;
	int key_pbe;
	char *keyname;
	int keytype;
	char *infile;
	int iter;
	char *macalg;
	int maciter;
	int macver;
	char *name;
	int noprompt;
	int options;
	char *outfile;
	char *passarg;
	char *passargin;
	char *passargout;
	int twopass;
} pkcs12_config;

static int
pkcs12_opt_canames(char *arg)
{
	if (pkcs12_config.canames == NULL &&
	    (pkcs12_config.canames = sk_OPENSSL_STRING_new_null()) == NULL)
		return (1);

	if (!sk_OPENSSL_STRING_push(pkcs12_config.canames, arg))
		return (1);

	return (0);
}

static int
pkcs12_opt_cert_pbe(char *arg)
{
	return (!set_pbe(bio_err, &pkcs12_config.cert_pbe, arg));
}

static int
pkcs12_opt_key_pbe(char *arg)
{
	return (!set_pbe(bio_err, &pkcs12_config.key_pbe, arg));
}

static int
pkcs12_opt_passarg(char *arg)
{
	pkcs12_config.passarg = arg;
	pkcs12_config.noprompt = 1;
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
pkcs12_opt_enc(int argc, char **argv, int *argsused)
{
	char *name = argv[0];

	if (*name++ != '-')
		return (1);

	if (strcmp(name, "nodes") == 0)
		pkcs12_config.enc = NULL;
	else if ((pkcs12_config.enc = get_cipher_by_name(name)) == NULL)
		return (1);

	*argsused = 1;
	return (0);
}

static const struct option pkcs12_options[] = {
#ifndef OPENSSL_NO_AES
	{
		.name = "aes128",
		.desc = "Encrypt PEM output with CBC AES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = pkcs12_opt_enc,
	},
	{
		.name = "aes192",
		.desc = "Encrypt PEM output with CBC AES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = pkcs12_opt_enc,
	},
	{
		.name = "aes256",
		.desc = "Encrypt PEM output with CBC AES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = pkcs12_opt_enc,
	},
#endif
#ifndef OPENSSL_NO_CAMELLIA
	{
		.name = "camellia128",
		.desc = "Encrypt PEM output with CBC Camellia",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = pkcs12_opt_enc,
	},
	{
		.name = "camellia192",
		.desc = "Encrypt PEM output with CBC Camellia",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = pkcs12_opt_enc,
	},
	{
		.name = "camellia256",
		.desc = "Encrypt PEM output with CBC Camellia",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = pkcs12_opt_enc,
	},
#endif
	{
		.name = "des",
		.desc = "Encrypt private keys with DES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = pkcs12_opt_enc,
	},
	{
		.name = "des3",
		.desc = "Encrypt private keys with triple DES (default)",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = pkcs12_opt_enc,
	},
#ifndef OPENSSL_NO_IDEA
	{
		.name = "idea",
		.desc = "Encrypt private keys with IDEA",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = pkcs12_opt_enc,
	},
#endif
	{
		.name = "cacerts",
		.desc = "Only output CA certificates",
		.type = OPTION_VALUE_OR,
		.opt.value = &pkcs12_config.options,
		.value = CACERTS,
	},
	{
		.name = "CAfile",
		.argname = "file",
		.desc = "PEM format file of CA certificates",
		.type = OPTION_ARG,
		.opt.arg = &pkcs12_config.CAfile,
	},
	{
		.name = "caname",
		.argname = "name",
		.desc = "Use name as CA friendly name (can be used more than once)",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = pkcs12_opt_canames,
	},
	{
		.name = "CApath",
		.argname = "directory",
		.desc = "PEM format directory of CA certificates",
		.type = OPTION_ARG,
		.opt.arg = &pkcs12_config.CApath,
	},
	{
		.name = "certfile",
		.argname = "file",
		.desc = "Add all certs in file",
		.type = OPTION_ARG,
		.opt.arg = &pkcs12_config.certfile,
	},
	{
		.name = "certpbe",
		.argname = "alg",
		.desc = "Specify certificate PBE algorithm (default RC2-40)",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = pkcs12_opt_cert_pbe,
	},
	{
		.name = "chain",
		.desc = "Add certificate chain",
		.type = OPTION_FLAG,
		.opt.flag = &pkcs12_config.chain,
	},
	{
		.name = "clcerts",
		.desc = "Only output client certificates",
		.type = OPTION_VALUE_OR,
		.opt.value = &pkcs12_config.options,
		.value = CLCERTS,
	},
	{
		.name = "CSP",
		.argname = "name",
		.desc = "Microsoft CSP name",
		.type = OPTION_ARG,
		.opt.arg = &pkcs12_config.csp_name,
	},
	{
		.name = "descert",
		.desc = "Encrypt PKCS#12 certificates with triple DES (default RC2-40)",
		.type = OPTION_VALUE,
		.opt.value = &pkcs12_config.cert_pbe,
		.value = NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
	},
	{
		.name = "export",
		.desc = "Output PKCS#12 file",
		.type = OPTION_FLAG,
		.opt.flag = &pkcs12_config.export_cert,
	},
	{
		.name = "in",
		.argname = "file",
		.desc = "Input filename",
		.type = OPTION_ARG,
		.opt.arg = &pkcs12_config.infile,
	},
	{
		.name = "info",
		.desc = "Give info about PKCS#12 structure",
		.type = OPTION_VALUE_OR,
		.opt.value = &pkcs12_config.options,
		.value = INFO,
	},
	{
		.name = "inkey",
		.argname = "file",
		.desc = "Private key if not infile",
		.type = OPTION_ARG,
		.opt.arg = &pkcs12_config.keyname,
	},
	{
		.name = "keyex",
		.desc = "Set MS key exchange type",
		.type = OPTION_VALUE,
		.opt.value = &pkcs12_config.keytype,
		.value = KEY_EX,
	},
	{
		.name = "keypbe",
		.argname = "alg",
		.desc = "Specify private key PBE algorithm (default 3DES)",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = pkcs12_opt_key_pbe,
	},
	{
		.name = "keysig",
		.desc = "Set MS key signature type",
		.type = OPTION_VALUE,
		.opt.value = &pkcs12_config.keytype,
		.value = KEY_SIG,
	},
	{
		.name = "LMK",
		.desc = "Add local machine keyset attribute to private key",
		.type = OPTION_FLAG,
		.opt.flag = &pkcs12_config.add_lmk,
	},
	{
		.name = "macalg",
		.argname = "alg",
		.desc = "Digest algorithm used in MAC (default SHA1)",
		.type = OPTION_ARG,
		.opt.arg = &pkcs12_config.macalg,
	},
	{
		.name = "maciter",
		.desc = "Use MAC iteration",
		.type = OPTION_VALUE,
		.opt.value = &pkcs12_config.maciter,
		.value = PKCS12_DEFAULT_ITER,
	},
	{
		.name = "name",
		.argname = "name",
		.desc = "Use name as friendly name",
		.type = OPTION_ARG,
		.opt.arg = &pkcs12_config.name,
	},
	{
		.name = "nocerts",
		.desc = "Don't output certificates",
		.type = OPTION_VALUE_OR,
		.opt.value = &pkcs12_config.options,
		.value = NOCERTS,
	},
	{
		.name = "nodes",
		.desc = "Don't encrypt private keys",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = pkcs12_opt_enc,
	},
	{
		.name = "noiter",
		.desc = "Don't use encryption iteration",
		.type = OPTION_VALUE,
		.opt.value = &pkcs12_config.iter,
		.value = 1,
	},
	{
		.name = "nokeys",
		.desc = "Don't output private keys",
		.type = OPTION_VALUE_OR,
		.opt.value = &pkcs12_config.options,
		.value = NOKEYS,
	},
	{
		.name = "nomac",
		.desc = "Don't generate MAC",
		.type = OPTION_VALUE,
		.opt.value = &pkcs12_config.maciter,
		.value = -1,
	},
	{
		.name = "nomaciter",
		.desc = "Don't use MAC iteration",
		.type = OPTION_VALUE,
		.opt.value = &pkcs12_config.maciter,
		.value = 1,
	},
	{
		.name = "nomacver",
		.desc = "Don't verify MAC",
		.type = OPTION_VALUE,
		.opt.value = &pkcs12_config.macver,
		.value = 0,
	},
	{
		.name = "noout",
		.desc = "Don't output anything, just verify",
		.type = OPTION_VALUE_OR,
		.opt.value = &pkcs12_config.options,
		.value = (NOKEYS | NOCERTS),
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output filename",
		.type = OPTION_ARG,
		.opt.arg = &pkcs12_config.outfile,
	},
	{
		.name = "passin",
		.argname = "arg",
		.desc = "Input file passphrase source",
		.type = OPTION_ARG,
		.opt.arg = &pkcs12_config.passargin,
	},
	{
		.name = "passout",
		.argname = "arg",
		.desc = "Output file passphrase source",
		.type = OPTION_ARG,
		.opt.arg = &pkcs12_config.passargout,
	},
	{
		.name = "password",
		.argname = "arg",
		.desc = "Set import/export password source",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = pkcs12_opt_passarg,
	},
	{
		.name = "twopass",
		.desc = "Separate MAC, encryption passwords",
		.type = OPTION_FLAG,
		.opt.flag = &pkcs12_config.twopass,
	},
	{ NULL },
};

static void
pkcs12_usage(void)
{
	fprintf(stderr, "usage: pkcs12 [-aes128 | -aes192 | -aes256 |");
	fprintf(stderr, " -camellia128 |\n");
	fprintf(stderr, "    -camellia192 | -camellia256 | -des | -des3 |");
	fprintf(stderr, " -idea]\n");
	fprintf(stderr, "    [-cacerts] [-CAfile file] [-caname name]\n");
	fprintf(stderr, "    [-CApath directory] [-certfile file]");
	fprintf(stderr, " [-certpbe alg]\n");
	fprintf(stderr, "    [-chain] [-clcerts] [-CSP name] [-descert]");
	fprintf(stderr, " [-export]\n");
	fprintf(stderr, "    [-in file] [-info] [-inkey file] [-keyex]");
	fprintf(stderr, " [-keypbe alg]\n");
	fprintf(stderr, "    [-keysig] [-LMK] [-macalg alg] [-maciter]");
	fprintf(stderr, " [-name name]\n");
	fprintf(stderr, "    [-nocerts] [-nodes] [-noiter] [-nokeys]");
	fprintf(stderr, " [-nomac]\n");
	fprintf(stderr, "    [-nomaciter] [-nomacver] [-noout] [-out file]\n");
	fprintf(stderr, "    [-passin arg] [-passout arg] [-password arg]");
	fprintf(stderr, " [-twopass]\n\n");
	options_usage(pkcs12_options);
	fprintf(stderr, "\n");
}

int
pkcs12_main(int argc, char **argv)
{
	BIO *in = NULL, *out = NULL;
	PKCS12 *p12 = NULL;
	char pass[50], macpass[50];
	int ret = 1;
	char *cpass = NULL, *mpass = NULL;
	char *passin = NULL, *passout = NULL;

	if (single_execution) {
		if (pledge("stdio cpath wpath rpath tty", NULL) == -1) {
			perror("pledge");
			exit(1);
		}
	}

	memset(&pkcs12_config, 0, sizeof(pkcs12_config));
	pkcs12_config.cert_pbe = NID_pbe_WithSHA1And40BitRC2_CBC;
	pkcs12_config.enc = EVP_des_ede3_cbc();
	pkcs12_config.iter = PKCS12_DEFAULT_ITER;
	pkcs12_config.key_pbe = NID_pbe_WithSHA1And3_Key_TripleDES_CBC;
	pkcs12_config.maciter = PKCS12_DEFAULT_ITER;
	pkcs12_config.macver = 1;

	if (options_parse(argc, argv, pkcs12_options, NULL, NULL) != 0) {
		pkcs12_usage();
		goto end;
	}

	if (pkcs12_config.passarg) {
		if (pkcs12_config.export_cert)
			pkcs12_config.passargout = pkcs12_config.passarg;
		else
			pkcs12_config.passargin = pkcs12_config.passarg;
	}
	if (!app_passwd(bio_err, pkcs12_config.passargin,
	    pkcs12_config.passargout, &passin, &passout)) {
		BIO_printf(bio_err, "Error getting passwords\n");
		goto end;
	}
	if (!cpass) {
		if (pkcs12_config.export_cert)
			cpass = passout;
		else
			cpass = passin;
	}
	if (cpass) {
		mpass = cpass;
		pkcs12_config.noprompt = 1;
	} else {
		cpass = pass;
		mpass = macpass;
	}

	if (!pkcs12_config.infile)
		in = BIO_new_fp(stdin, BIO_NOCLOSE);
	else
		in = BIO_new_file(pkcs12_config.infile, "rb");
	if (!in) {
		BIO_printf(bio_err, "Error opening input file %s\n",
		    pkcs12_config.infile ? pkcs12_config.infile : "<stdin>");
		perror(pkcs12_config.infile);
		goto end;
	}

	if (!pkcs12_config.outfile) {
		out = BIO_new_fp(stdout, BIO_NOCLOSE);
	} else
		out = BIO_new_file(pkcs12_config.outfile, "wb");
	if (!out) {
		BIO_printf(bio_err, "Error opening output file %s\n",
		    pkcs12_config.outfile ? pkcs12_config.outfile : "<stdout>");
		perror(pkcs12_config.outfile);
		goto end;
	}
	if (pkcs12_config.twopass) {
		if (EVP_read_pw_string(macpass, sizeof macpass,
		    "Enter MAC Password:", pkcs12_config.export_cert)) {
			BIO_printf(bio_err, "Can't read Password\n");
			goto end;
		}
	}
	if (pkcs12_config.export_cert) {
		EVP_PKEY *key = NULL;
		X509 *ucert = NULL, *x = NULL;
		STACK_OF(X509) *certs = NULL;
		const EVP_MD *macmd = NULL;
		unsigned char *catmp = NULL;
		int i;

		if ((pkcs12_config.options & (NOCERTS | NOKEYS)) ==
		    (NOCERTS | NOKEYS)) {
			BIO_printf(bio_err, "Nothing to do!\n");
			goto export_end;
		}
		if (pkcs12_config.options & NOCERTS)
			pkcs12_config.chain = 0;

		if (!(pkcs12_config.options & NOKEYS)) {
			key = load_key(bio_err, pkcs12_config.keyname ?
			    pkcs12_config.keyname : pkcs12_config.infile,
			    FORMAT_PEM, 1, passin, "private key");
			if (!key)
				goto export_end;
		}

		/* Load in all certs in input file */
		if (!(pkcs12_config.options & NOCERTS)) {
			certs = load_certs(bio_err, pkcs12_config.infile,
			    FORMAT_PEM, NULL, "certificates");
			if (!certs)
				goto export_end;

			if (key) {
				/* Look for matching private key */
				for (i = 0; i < sk_X509_num(certs); i++) {
					x = sk_X509_value(certs, i);
					if (X509_check_private_key(x, key)) {
						ucert = x;
						/* Zero keyid and alias */
						X509_keyid_set1(ucert, NULL, 0);
						X509_alias_set1(ucert, NULL, 0);
						/* Remove from list */
						(void) sk_X509_delete(certs, i);
						break;
					}
				}
				if (!ucert) {
					BIO_printf(bio_err,
					    "No certificate matches private key\n");
					goto export_end;
				}
			}
		}

		/* Add any more certificates asked for */
		if (pkcs12_config.certfile) {
			STACK_OF(X509) *morecerts = NULL;
			if (!(morecerts = load_certs(bio_err,
			    pkcs12_config.certfile, FORMAT_PEM, NULL,
			    "certificates from certfile")))
				goto export_end;
			while (sk_X509_num(morecerts) > 0)
				sk_X509_push(certs, sk_X509_shift(morecerts));
			sk_X509_free(morecerts);
		}


		/* If chaining get chain from user cert */
		if (pkcs12_config.chain) {
			int vret;
			STACK_OF(X509) *chain2;
			X509_STORE *store = X509_STORE_new();
			if (!store) {
				BIO_printf(bio_err,
				    "Memory allocation error\n");
				goto export_end;
			}
			if (!X509_STORE_load_locations(store,
			    pkcs12_config.CAfile, pkcs12_config.CApath))
				X509_STORE_set_default_paths(store);

			vret = get_cert_chain(ucert, store, &chain2);
			X509_STORE_free(store);

			if (!vret) {
				/* Exclude verified certificate */
				for (i = 1; i < sk_X509_num(chain2); i++)
					sk_X509_push(certs, sk_X509_value(
					    chain2, i));
				/* Free first certificate */
				X509_free(sk_X509_value(chain2, 0));
				sk_X509_free(chain2);
			} else {
				if (vret >= 0)
					BIO_printf(bio_err,
					    "Error %s getting chain.\n",
					    X509_verify_cert_error_string(
					    vret));
				else
					ERR_print_errors(bio_err);
				goto export_end;
			}
		}
		/* Add any CA names */

		for (i = 0; i < sk_OPENSSL_STRING_num(pkcs12_config.canames);
		    i++) {
			catmp = (unsigned char *) sk_OPENSSL_STRING_value(
			    pkcs12_config.canames, i);
			X509_alias_set1(sk_X509_value(certs, i), catmp, -1);
		}

		if (pkcs12_config.csp_name && key)
			EVP_PKEY_add1_attr_by_NID(key, NID_ms_csp_name,
			    MBSTRING_ASC,
			    (unsigned char *) pkcs12_config.csp_name, -1);

		if (pkcs12_config.add_lmk && key)
			EVP_PKEY_add1_attr_by_NID(key, NID_LocalKeySet, 0, NULL,
			    -1);

		if (!pkcs12_config.noprompt &&
		    EVP_read_pw_string(pass, sizeof pass,
		    "Enter Export Password:", 1)) {
			BIO_printf(bio_err, "Can't read Password\n");
			goto export_end;
		}
		if (!pkcs12_config.twopass)
			strlcpy(macpass, pass, sizeof macpass);


		p12 = PKCS12_create(cpass, pkcs12_config.name, key, ucert,
		    certs, pkcs12_config.key_pbe, pkcs12_config.cert_pbe,
		    pkcs12_config.iter, -1, pkcs12_config.keytype);

		if (!p12) {
			ERR_print_errors(bio_err);
			goto export_end;
		}
		if (pkcs12_config.macalg) {
			macmd = EVP_get_digestbyname(pkcs12_config.macalg);
			if (!macmd) {
				BIO_printf(bio_err,
				    "Unknown digest algorithm %s\n",
				    pkcs12_config.macalg);
			}
		}
		if (pkcs12_config.maciter != -1)
			PKCS12_set_mac(p12, mpass, -1, NULL, 0,
			    pkcs12_config.maciter, macmd);

		i2d_PKCS12_bio(out, p12);

		ret = 0;

 export_end:
		EVP_PKEY_free(key);
		sk_X509_pop_free(certs, X509_free);
		X509_free(ucert);

		goto end;

	}
	if (!(p12 = d2i_PKCS12_bio(in, NULL))) {
		ERR_print_errors(bio_err);
		goto end;
	}
	if (!pkcs12_config.noprompt && EVP_read_pw_string(pass, sizeof pass,
	    "Enter Import Password:", 0)) {
		BIO_printf(bio_err, "Can't read Password\n");
		goto end;
	}

	if (!pkcs12_config.twopass)
		strlcpy(macpass, pass, sizeof macpass);

	if ((pkcs12_config.options & INFO) && p12->mac)
		BIO_printf(bio_err, "MAC Iteration %ld\n",
		    p12->mac->iter ? ASN1_INTEGER_get(p12->mac->iter) : 1);
	if (pkcs12_config.macver) {
		/* If we enter empty password try no password first */
		if (!mpass[0] && PKCS12_verify_mac(p12, NULL, 0)) {
			/* If mac and crypto pass the same set it to NULL too */
			if (!pkcs12_config.twopass)
				cpass = NULL;
		} else if (!PKCS12_verify_mac(p12, mpass, -1)) {
			BIO_printf(bio_err,
			    "Mac verify error: invalid password?\n");
			ERR_print_errors(bio_err);
			goto end;
		}
		BIO_printf(bio_err, "MAC verified OK\n");
	}
	if (!dump_certs_keys_p12(out, p12, cpass, -1, pkcs12_config.options,
	    passout)) {
		BIO_printf(bio_err, "Error outputting keys and certificates\n");
		ERR_print_errors(bio_err);
		goto end;
	}
	ret = 0;
 end:
	PKCS12_free(p12);
	BIO_free(in);
	BIO_free_all(out);
	sk_OPENSSL_STRING_free(pkcs12_config.canames);
	free(passin);
	free(passout);

	return (ret);
}

int
dump_certs_keys_p12(BIO *out, PKCS12 *p12, char *pass,
    int passlen, int options, char *pempass)
{
	STACK_OF(PKCS7) *asafes = NULL;
	STACK_OF(PKCS12_SAFEBAG) *bags;
	int i, bagnid;
	int ret = 0;
	PKCS7 *p7;

	if (!(asafes = PKCS12_unpack_authsafes(p12)))
		return 0;
	for (i = 0; i < sk_PKCS7_num(asafes); i++) {
		p7 = sk_PKCS7_value(asafes, i);
		bagnid = OBJ_obj2nid(p7->type);
		if (bagnid == NID_pkcs7_data) {
			bags = PKCS12_unpack_p7data(p7);
			if (options & INFO)
				BIO_printf(bio_err, "PKCS7 Data\n");
		} else if (bagnid == NID_pkcs7_encrypted) {
			if (options & INFO) {
				BIO_printf(bio_err, "PKCS7 Encrypted data: ");
				alg_print(bio_err,
				    p7->d.encrypted->enc_data->algorithm);
			}
			bags = PKCS12_unpack_p7encdata(p7, pass, passlen);
		} else
			continue;
		if (!bags)
			goto err;
		if (!dump_certs_pkeys_bags(out, bags, pass, passlen,
			options, pempass)) {
			sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
			goto err;
		}
		sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
		bags = NULL;
	}
	ret = 1;

 err:
	sk_PKCS7_pop_free(asafes, PKCS7_free);
	return ret;
}

int
dump_certs_pkeys_bags(BIO *out, STACK_OF(PKCS12_SAFEBAG) *bags,
    char *pass, int passlen, int options, char *pempass)
{
	int i;
	for (i = 0; i < sk_PKCS12_SAFEBAG_num(bags); i++) {
		if (!dump_certs_pkeys_bag(out,
			sk_PKCS12_SAFEBAG_value(bags, i),
			pass, passlen,
			options, pempass))
			return 0;
	}
	return 1;
}

int
dump_certs_pkeys_bag(BIO *out, PKCS12_SAFEBAG *bag, char *pass,
    int passlen, int options, char *pempass)
{
	EVP_PKEY *pkey;
	PKCS8_PRIV_KEY_INFO *p8;
	X509 *x509;

	switch (OBJ_obj2nid(bag->type)) {
	case NID_keyBag:
		if (options & INFO)
			BIO_printf(bio_err, "Key bag\n");
		if (options & NOKEYS)
			return 1;
		print_attribs(out, bag->attrib, "Bag Attributes");
		p8 = bag->value.keybag;
		if (!(pkey = EVP_PKCS82PKEY(p8)))
			return 0;
		print_attribs(out, p8->attributes, "Key Attributes");
		PEM_write_bio_PrivateKey(out, pkey, pkcs12_config.enc, NULL, 0,
		    NULL, pempass);
		EVP_PKEY_free(pkey);
		break;

	case NID_pkcs8ShroudedKeyBag:
		if (options & INFO) {
			BIO_printf(bio_err, "Shrouded Keybag: ");
			alg_print(bio_err, bag->value.shkeybag->algor);
		}
		if (options & NOKEYS)
			return 1;
		print_attribs(out, bag->attrib, "Bag Attributes");
		if (!(p8 = PKCS12_decrypt_skey(bag, pass, passlen)))
			return 0;
		if (!(pkey = EVP_PKCS82PKEY(p8))) {
			PKCS8_PRIV_KEY_INFO_free(p8);
			return 0;
		}
		print_attribs(out, p8->attributes, "Key Attributes");
		PKCS8_PRIV_KEY_INFO_free(p8);
		PEM_write_bio_PrivateKey(out, pkey, pkcs12_config.enc, NULL, 0,
		    NULL, pempass);
		EVP_PKEY_free(pkey);
		break;

	case NID_certBag:
		if (options & INFO)
			BIO_printf(bio_err, "Certificate bag\n");
		if (options & NOCERTS)
			return 1;
		if (PKCS12_get_attr(bag, NID_localKeyID)) {
			if (options & CACERTS)
				return 1;
		} else if (options & CLCERTS)
			return 1;
		print_attribs(out, bag->attrib, "Bag Attributes");
		if (OBJ_obj2nid(bag->value.bag->type) != NID_x509Certificate)
			return 1;
		if (!(x509 = PKCS12_certbag2x509(bag)))
			return 0;
		dump_cert_text(out, x509);
		PEM_write_bio_X509(out, x509);
		X509_free(x509);
		break;

	case NID_safeContentsBag:
		if (options & INFO)
			BIO_printf(bio_err, "Safe Contents bag\n");
		print_attribs(out, bag->attrib, "Bag Attributes");
		return dump_certs_pkeys_bags(out, bag->value.safes, pass,
		    passlen, options, pempass);

	default:
		BIO_printf(bio_err, "Warning unsupported bag type: ");
		i2a_ASN1_OBJECT(bio_err, bag->type);
		BIO_printf(bio_err, "\n");
		return 1;
		break;
	}
	return 1;
}

/* Given a single certificate return a verified chain or NULL if error */

/* Hope this is OK .... */

int
get_cert_chain(X509 *cert, X509_STORE *store, STACK_OF(X509) **chain)
{
	X509_STORE_CTX store_ctx;
	STACK_OF(X509) *chn;
	int i = 0;

	/*
	 * FIXME: Should really check the return status of
	 * X509_STORE_CTX_init for an error, but how that fits into the
	 * return value of this function is less obvious.
	 */
	X509_STORE_CTX_init(&store_ctx, store, cert, NULL);
	if (X509_verify_cert(&store_ctx) <= 0) {
		i = X509_STORE_CTX_get_error(&store_ctx);
		if (i == 0)
			/*
			 * avoid returning 0 if X509_verify_cert() did not
			 * set an appropriate error value in the context
			 */
			i = -1;
		chn = NULL;
		goto err;
	} else
		chn = X509_STORE_CTX_get1_chain(&store_ctx);
 err:
	X509_STORE_CTX_cleanup(&store_ctx);
	*chain = chn;

	return i;
}

int
alg_print(BIO *x, X509_ALGOR *alg)
{
	PBEPARAM *pbe;
	const unsigned char *p;
	p = alg->parameter->value.sequence->data;
	pbe = d2i_PBEPARAM(NULL, &p, alg->parameter->value.sequence->length);
	if (!pbe)
		return 1;
	BIO_printf(bio_err, "%s, Iteration %ld\n",
	    OBJ_nid2ln(OBJ_obj2nid(alg->algorithm)),
	    ASN1_INTEGER_get(pbe->iter));
	PBEPARAM_free(pbe);
	return 1;
}

/* Load all certificates from a given file */

int
cert_load(BIO *in, STACK_OF(X509) *sk)
{
	int ret;
	X509 *cert;
	ret = 0;
	while ((cert = PEM_read_bio_X509(in, NULL, NULL, NULL))) {
		ret = 1;
		sk_X509_push(sk, cert);
	}
	if (ret)
		ERR_clear_error();
	return ret;
}

/* Generalised attribute print: handle PKCS#8 and bag attributes */

int
print_attribs(BIO *out, STACK_OF(X509_ATTRIBUTE) *attrlst, const char *name)
{
	X509_ATTRIBUTE *attr;
	ASN1_TYPE *av;
	char *value;
	int i, attr_nid;
	if (!attrlst) {
		BIO_printf(out, "%s: <No Attributes>\n", name);
		return 1;
	}
	if (!sk_X509_ATTRIBUTE_num(attrlst)) {
		BIO_printf(out, "%s: <Empty Attributes>\n", name);
		return 1;
	}
	BIO_printf(out, "%s\n", name);
	for (i = 0; i < sk_X509_ATTRIBUTE_num(attrlst); i++) {
		attr = sk_X509_ATTRIBUTE_value(attrlst, i);
		attr_nid = OBJ_obj2nid(attr->object);
		BIO_printf(out, "    ");
		if (attr_nid == NID_undef) {
			i2a_ASN1_OBJECT(out, attr->object);
			BIO_printf(out, ": ");
		} else
			BIO_printf(out, "%s: ", OBJ_nid2ln(attr_nid));

		if (sk_ASN1_TYPE_num(attr->value.set)) {
			av = sk_ASN1_TYPE_value(attr->value.set, 0);
			switch (av->type) {
			case V_ASN1_BMPSTRING:
				value = OPENSSL_uni2asc(
				    av->value.bmpstring->data,
				    av->value.bmpstring->length);
				BIO_printf(out, "%s\n", value);
				free(value);
				break;

			case V_ASN1_OCTET_STRING:
				hex_prin(out, av->value.octet_string->data,
				    av->value.octet_string->length);
				BIO_printf(out, "\n");
				break;

			case V_ASN1_BIT_STRING:
				hex_prin(out, av->value.bit_string->data,
				    av->value.bit_string->length);
				BIO_printf(out, "\n");
				break;

			default:
				BIO_printf(out, "<Unsupported tag %d>\n",
				    av->type);
				break;
			}
		} else
			BIO_printf(out, "<No Values>\n");
	}
	return 1;
}

void
hex_prin(BIO *out, unsigned char *buf, int len)
{
	int i;
	for (i = 0; i < len; i++)
		BIO_printf(out, "%02X ", buf[i]);
}

static int
set_pbe(BIO *err, int *ppbe, const char *str)
{
	if (!str)
		return 0;
	if (!strcmp(str, "NONE")) {
		*ppbe = -1;
		return 1;
	}
	*ppbe = OBJ_txt2nid(str);
	if (*ppbe == NID_undef) {
		BIO_printf(bio_err, "Unknown PBE algorithm %s\n", str);
		return 0;
	}
	return 1;
}

#endif
