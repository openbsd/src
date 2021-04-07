/* $OpenBSD: x509.c,v 1.23 2021/04/07 10:44:03 inoguchi Exp $ */
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

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apps.h"

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/dsa.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#define	POSTFIX	".srl"
#define DEF_DAYS	30

static int callb(int ok, X509_STORE_CTX *ctx);
static int sign(X509 *x, EVP_PKEY *pkey, int days, int clrext,
    const EVP_MD *digest, CONF *conf, char *section);
static int x509_certify(X509_STORE *ctx, char *CAfile, const EVP_MD *digest,
    X509 *x, X509 *xca, EVP_PKEY *pkey, STACK_OF(OPENSSL_STRING) *sigopts,
    char *serial, int create, int days, int clrext, CONF *conf, char *section,
    ASN1_INTEGER *sno);
static int purpose_print(BIO *bio, X509 *cert, X509_PURPOSE *pt);

static struct {
	char *alias;
	int aliasout;
	int badops;
	int C;
	int CA_createserial;
	int CA_flag;
	char *CAfile;
	int CAformat;
	char *CAkeyfile;
	int CAkeyformat;
	char *CAserial;
	unsigned long certflag;
	int checkend;
	int checkoffset;
	int clrext;
	int clrreject;
	int clrtrust;
	int days;
	const EVP_MD *digest;
	int email;
	int enddate;
	char *extfile;
	char *extsect;
	int fingerprint;
	char *infile;
	int informat;
	int issuer;
	int issuer_hash;
#ifndef OPENSSL_NO_MD5
	int issuer_hash_old;
#endif
	char *keyfile;
	int keyformat;
	const EVP_MD *md_alg;
	int modulus;
	int next_serial;
	unsigned long nmflag;
	int noout;
	int num;
	int ocspid;
	ASN1_OBJECT *objtmp;
	int ocsp_uri;
	char *outfile;
	int outformat;
	char *passargin;
	int pprint;
	int pubkey;
	STACK_OF(ASN1_OBJECT) *reject;
	int reqfile;
	int serial;
	int sign_flag;
	STACK_OF(OPENSSL_STRING) *sigopts;
	ASN1_INTEGER *sno;
	int startdate;
	int subject;
	int subject_hash;
#ifndef OPENSSL_NO_MD5
	int subject_hash_old;
#endif
	int text;
	STACK_OF(ASN1_OBJECT) *trust;
	int trustout;
	int x509req;
} x509_config;

static int
x509_opt_addreject(char *arg)
{
	if ((x509_config.objtmp = OBJ_txt2obj(arg, 0)) == NULL) {
		BIO_printf(bio_err, "Invalid reject object value %s\n", arg);
		return (1);
	}

	if (x509_config.reject == NULL &&
	    (x509_config.reject = sk_ASN1_OBJECT_new_null()) == NULL)
		return (1);

	if (!sk_ASN1_OBJECT_push(x509_config.reject, x509_config.objtmp))
		return (1);

	x509_config.trustout = 1;
	return (0);
}

static int
x509_opt_addtrust(char *arg)
{
	if ((x509_config.objtmp = OBJ_txt2obj(arg, 0)) == NULL) {
		BIO_printf(bio_err, "Invalid trust object value %s\n", arg);
		return (1);
	}

	if (x509_config.trust == NULL &&
	    (x509_config.trust = sk_ASN1_OBJECT_new_null()) == NULL)
		return (1);

	if (!sk_ASN1_OBJECT_push(x509_config.trust, x509_config.objtmp))
		return (1);

	x509_config.trustout = 1;
	return (0);
}

static int
x509_opt_ca(char *arg)
{
	x509_config.CAfile = arg;
	x509_config.CA_flag = ++x509_config.num;
	return (0);
}

static int
x509_opt_certopt(char *arg)
{
	if (!set_cert_ex(&x509_config.certflag, arg))
		return (1);

	return (0);
}

static int
x509_opt_checkend(char *arg)
{
	const char *errstr;

	x509_config.checkoffset = strtonum(arg, 0, INT_MAX, &errstr);
	if (errstr != NULL) {
		BIO_printf(bio_err, "checkend unusable: %s\n", errstr);
		return (1);
	}
	x509_config.checkend = 1;
	return (0);
}

static int
x509_opt_dates(void)
{
	x509_config.startdate = ++x509_config.num;
	x509_config.enddate = ++x509_config.num;
	return (0);
}

static int
x509_opt_days(char *arg)
{
	const char *errstr;

	x509_config.days = strtonum(arg, 1, INT_MAX, &errstr);
	if (errstr != NULL) {
		BIO_printf(bio_err, "bad number of days: %s\n", errstr);
		return (1);
	}
	return (0);
}

static int
x509_opt_digest(int argc, char **argv, int *argsused)
{
	char *name = argv[0];

	if (*name++ != '-')
		return (1);

	if ((x509_config.md_alg = EVP_get_digestbyname(name)) != NULL) {
		x509_config.digest = x509_config.md_alg;
	} else {
		BIO_printf(bio_err, "unknown option %s\n", *argv);
		x509_config.badops = 1;
		return (1);
	}

	*argsused = 1;
	return (0);
}

static int
x509_opt_nameopt(char *arg)
{
	if (!set_name_ex(&x509_config.nmflag, arg))
		return (1);

	return (0);
}

static int
x509_opt_set_serial(char *arg)
{
	ASN1_INTEGER_free(x509_config.sno);
	if ((x509_config.sno = s2i_ASN1_INTEGER(NULL, arg)) == NULL)
		return (1);

	return (0);
}

static int
x509_opt_setalias(char *arg)
{
	x509_config.alias = arg;
	x509_config.trustout = 1;
	return (0);
}

static int
x509_opt_signkey(char *arg)
{
	x509_config.keyfile = arg;
	x509_config.sign_flag = ++x509_config.num;
	return (0);
}

static int
x509_opt_sigopt(char *arg)
{
	if (x509_config.sigopts == NULL &&
	    (x509_config.sigopts = sk_OPENSSL_STRING_new_null()) == NULL)
		return (1);

	if (!sk_OPENSSL_STRING_push(x509_config.sigopts, arg))
		return (1);

	return (0);
}

static const struct option x509_options[] = {
	{
		.name = "C",
		.desc = "Convert the certificate into C code",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.C,
		.order = &x509_config.num,
	},
	{
		.name = "addreject",
		.argname = "arg",
		.desc = "Reject certificate for a given purpose",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = x509_opt_addreject,
	},
	{
		.name = "addtrust",
		.argname = "arg",
		.desc = "Trust certificate for a given purpose",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = x509_opt_addtrust,
	},
	{
		.name = "alias",
		.desc = "Output certificate alias",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.aliasout,
		.order = &x509_config.num,
	},
	{
		.name = "CA",
		.argname = "file",
		.desc = "CA certificate in PEM format unless -CAform is specified",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = x509_opt_ca,
	},
	{
		.name = "CAcreateserial",
		.desc = "Create serial number file if it does not exist",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.CA_createserial,
		.order = &x509_config.num,
	},
	{
		.name = "CAform",
		.argname = "fmt",
		.desc = "CA format - default PEM",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &x509_config.CAformat,
	},
	{
		.name = "CAkey",
		.argname = "file",
		.desc = "CA key in PEM format unless -CAkeyform is specified\n"
			"if omitted, the key is assumed to be in the CA file",
		.type = OPTION_ARG,
		.opt.arg = &x509_config.CAkeyfile,
	},
	{
		.name = "CAkeyform",
		.argname = "fmt",
		.desc = "CA key format - default PEM",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &x509_config.CAkeyformat,
	},
	{
		.name = "CAserial",
		.argname = "file",
		.desc = "Serial file",
		.type = OPTION_ARG,
		.opt.arg = &x509_config.CAserial,
	},
	{
		.name = "certopt",
		.argname = "option",
		.desc = "Various certificate text options",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = x509_opt_certopt,
	},
	{
		.name = "checkend",
		.argname = "arg",
		.desc = "Check whether the cert expires in the next arg seconds\n"
			"exit 1 if so, 0 if not",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = x509_opt_checkend,
	},
	{
		.name = "clrext",
		.desc = "Clear all extensions",
		.type = OPTION_FLAG,
		.opt.flag = &x509_config.clrext,
	},
	{
		.name = "clrreject",
		.desc = "Clear all rejected purposes",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.clrreject,
		.order = &x509_config.num,
	},
	{
		.name = "clrtrust",
		.desc = "Clear all trusted purposes",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.clrtrust,
		.order = &x509_config.num,
	},
	{
		.name = "dates",
		.desc = "Both Before and After dates",
		.type = OPTION_FUNC,
		.opt.func = x509_opt_dates,
	},
	{
		.name = "days",
		.argname = "arg",
		.desc = "How long till expiry of a signed certificate - def 30 days",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = x509_opt_days,
	},
	{
		.name = "email",
		.desc = "Print email address(es)",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.email,
		.order = &x509_config.num,
	},
	{
		.name = "enddate",
		.desc = "Print notAfter field",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.enddate,
		.order = &x509_config.num,
	},
	{
		.name = "extensions",
		.argname = "section",
		.desc = "Section from config file with X509V3 extensions to add",
		.type = OPTION_ARG,
		.opt.arg = &x509_config.extsect,
	},
	{
		.name = "extfile",
		.argname = "file",
		.desc = "Configuration file with X509V3 extensions to add",
		.type = OPTION_ARG,
		.opt.arg = &x509_config.extfile,
	},
	{
		.name = "fingerprint",
		.desc = "Print the certificate fingerprint",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.fingerprint,
		.order = &x509_config.num,
	},
	{
		.name = "hash",
		.desc = "Synonym for -subject_hash",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.subject_hash,
		.order = &x509_config.num,
	},
	{
		.name = "in",
		.argname = "file",
		.desc = "Input file - default stdin",
		.type = OPTION_ARG,
		.opt.arg = &x509_config.infile,
	},
	{
		.name = "inform",
		.argname = "fmt",
		.desc = "Input format - default PEM (one of DER, NET or PEM)",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &x509_config.informat,
	},
	{
		.name = "issuer",
		.desc = "Print issuer name",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.issuer,
		.order = &x509_config.num,
	},
	{
		.name = "issuer_hash",
		.desc = "Print issuer hash value",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.issuer_hash,
		.order = &x509_config.num,
	},
#ifndef OPENSSL_NO_MD5
	{
		.name = "issuer_hash_old",
		.desc = "Print old-style (MD5) issuer hash value",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.issuer_hash_old,
		.order = &x509_config.num,
	},
#endif
	{
		.name = "keyform",
		.argname = "fmt",
		.desc = "Private key format - default PEM",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &x509_config.keyformat,
	},
	{
		.name = "modulus",
		.desc = "Print the RSA key modulus",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.modulus,
		.order = &x509_config.num,
	},
	{
		.name = "nameopt",
		.argname = "option",
		.desc = "Various certificate name options",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = x509_opt_nameopt,
	},
	{
		.name = "next_serial",
		.desc = "Print the next serial number",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.next_serial,
		.order = &x509_config.num,
	},
	{
		.name = "noout",
		.desc = "No certificate output",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.noout,
		.order = &x509_config.num,
	},
	{
		.name = "ocsp_uri",
		.desc = "Print OCSP Responder URL(s)",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.ocsp_uri,
		.order = &x509_config.num,
	},
	{
		.name = "ocspid",
		.desc = "Print OCSP hash values for the subject name and public key",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.ocspid,
		.order = &x509_config.num,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output file - default stdout",
		.type = OPTION_ARG,
		.opt.arg = &x509_config.outfile,
	},
	{
		.name = "outform",
		.argname = "fmt",
		.desc = "Output format - default PEM (one of DER, NET or PEM)",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &x509_config.outformat,
	},
	{
		.name = "passin",
		.argname = "src",
		.desc = "Private key password source",
		.type = OPTION_ARG,
		.opt.arg = &x509_config.passargin,
	},
	{
		.name = "pubkey",
		.desc = "Output the public key",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.pubkey,
		.order = &x509_config.num,
	},
	{
		.name = "purpose",
		.desc = "Print out certificate purposes",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.pprint,
		.order = &x509_config.num,
	},
	{
		.name = "req",
		.desc = "Input is a certificate request, sign and output",
		.type = OPTION_FLAG,
		.opt.flag = &x509_config.reqfile,
	},
	{
		.name = "serial",
		.desc = "Print serial number value",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.serial,
		.order = &x509_config.num,
	},
	{
		.name = "set_serial",
		.argname = "n",
		.desc = "Serial number to use",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = x509_opt_set_serial,
	},
	{
		.name = "setalias",
		.argname = "arg",
		.desc = "Set certificate alias",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = x509_opt_setalias,
	},
	{
		.name = "signkey",
		.argname = "file",
		.desc = "Self sign cert with arg",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = x509_opt_signkey,
	},
	{
		.name = "sigopt",
		.argname = "nm:v",
		.desc = "Various signature algorithm options",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = x509_opt_sigopt,
	},
	{
		.name = "startdate",
		.desc = "Print notBefore field",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.startdate,
		.order = &x509_config.num,
	},
	{
		.name = "subject",
		.desc = "Print subject name",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.subject,
		.order = &x509_config.num,
	},
	{
		.name = "subject_hash",
		.desc = "Print subject hash value",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.subject_hash,
		.order = &x509_config.num,
	},
#ifndef OPENSSL_NO_MD5
	{
		.name = "subject_hash_old",
		.desc = "Print old-style (MD5) subject hash value",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.subject_hash_old,
		.order = &x509_config.num,
	},
#endif
	{
		.name = "text",
		.desc = "Print the certificate in text form",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.text,
		.order = &x509_config.num,
	},
	{
		.name = "trustout",
		.desc = "Output a trusted certificate",
		.type = OPTION_FLAG,
		.opt.flag = &x509_config.trustout,
	},
	{
		.name = "x509toreq",
		.desc = "Output a certification request object",
		.type = OPTION_ORDER,
		.opt.order = &x509_config.x509req,
		.order = &x509_config.num,
	},
	{
		.name = NULL,
		.desc = "",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = x509_opt_digest,
	},
	{ NULL },
};

static void
x509_usage(void)
{
	fprintf(stderr, "usage: x509 "
	    "[-C] [-addreject arg] [-addtrust arg] [-alias] [-CA file]\n"
	    "    [-CAcreateserial] [-CAform der | pem] [-CAkey file]\n"
	    "    [-CAkeyform der | pem] [-CAserial file] [-certopt option]\n"
	    "    [-checkend arg] [-clrext] [-clrreject] [-clrtrust] [-dates]\n"
	    "    [-days arg] [-email] [-enddate] [-extensions section]\n"
	    "    [-extfile file] [-fingerprint] [-hash] [-in file]\n"
	    "    [-inform der | net | pem] [-issuer] [-issuer_hash]\n"
	    "    [-issuer_hash_old] [-keyform der | pem] [-md5 | -sha1]\n"
	    "    [-modulus] [-nameopt option] [-next_serial] [-noout]\n"
	    "    [-ocsp_uri] [-ocspid] [-out file]\n"
	    "    [-outform der | net | pem] [-passin arg] [-pubkey]\n"
	    "    [-purpose] [-req] [-serial] [-set_serial n] [-setalias arg]\n"
	    "    [-signkey file] [-sigopt nm:v] [-startdate] [-subject]\n"
	    "    [-subject_hash] [-subject_hash_old] [-text] [-trustout]\n"
	    "    [-x509toreq]\n");
	fprintf(stderr, "\n");
	options_usage(x509_options);
	fprintf(stderr, "\n");
}

int
x509_main(int argc, char **argv)
{
	int ret = 1;
	X509_REQ *req = NULL;
	X509 *x = NULL, *xca = NULL;
	EVP_PKEY *Upkey = NULL, *CApkey = NULL;
	int i;
	BIO *out = NULL;
	BIO *STDout = NULL;
	X509_STORE *ctx = NULL;
	X509_REQ *rq = NULL;
	char buf[256];
	CONF *extconf = NULL;
	char *passin = NULL;

	if (single_execution) {
		if (pledge("stdio cpath wpath rpath tty", NULL) == -1) {
			perror("pledge");
			exit(1);
		}
	}

	memset(&x509_config, 0, sizeof(x509_config));
	x509_config.days = DEF_DAYS;
	x509_config.informat = FORMAT_PEM;
	x509_config.outformat = FORMAT_PEM;
	x509_config.keyformat = FORMAT_PEM;
	x509_config.CAformat = FORMAT_PEM;
	x509_config.CAkeyformat = FORMAT_PEM;

	STDout = BIO_new_fp(stdout, BIO_NOCLOSE);

	ctx = X509_STORE_new();
	if (ctx == NULL)
		goto end;
	X509_STORE_set_verify_cb(ctx, callb);

	if (options_parse(argc, argv, x509_options, NULL, NULL) != 0)
		goto bad;

	if (x509_config.badops) {
 bad:
		x509_usage();
		goto end;
	}

	if (!app_passwd(bio_err, x509_config.passargin, NULL, &passin, NULL)) {
		BIO_printf(bio_err, "Error getting password\n");
		goto end;
	}
	if (!X509_STORE_set_default_paths(ctx)) {
		ERR_print_errors(bio_err);
		goto end;
	}
	if ((x509_config.CAkeyfile == NULL) && (x509_config.CA_flag) &&
	    (x509_config.CAformat == FORMAT_PEM)) {
		x509_config.CAkeyfile = x509_config.CAfile;
	} else if ((x509_config.CA_flag) && (x509_config.CAkeyfile == NULL)) {
		BIO_printf(bio_err,
		    "need to specify a CAkey if using the CA command\n");
		goto end;
	}
	if (x509_config.extfile != NULL) {
		long errorline = -1;
		X509V3_CTX ctx2;
		extconf = NCONF_new(NULL);
		if (!NCONF_load(extconf, x509_config.extfile, &errorline)) {
			if (errorline <= 0)
				BIO_printf(bio_err,
				    "error loading the config file '%s'\n",
				    x509_config.extfile);
			else
				BIO_printf(bio_err,
				    "error on line %ld of config file '%s'\n",
				    errorline, x509_config.extfile);
			goto end;
		}
		if (x509_config.extsect == NULL) {
			x509_config.extsect = NCONF_get_string(extconf,
			    "default", "extensions");
			if (x509_config.extsect == NULL) {
				ERR_clear_error();
				x509_config.extsect = "default";
			}
		}
		X509V3_set_ctx_test(&ctx2);
		X509V3_set_nconf(&ctx2, extconf);
		if (!X509V3_EXT_add_nconf(extconf, &ctx2, x509_config.extsect,
		    NULL)) {
			BIO_printf(bio_err,
			    "Error Loading extension section %s\n",
			    x509_config.extsect);
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	if (x509_config.reqfile) {
		EVP_PKEY *pkey;
		BIO *in;

		if (!x509_config.sign_flag && !x509_config.CA_flag) {
			BIO_printf(bio_err,
			    "We need a private key to sign with\n");
			goto end;
		}
		in = BIO_new(BIO_s_file());
		if (in == NULL) {
			ERR_print_errors(bio_err);
			goto end;
		}
		if (x509_config.infile == NULL)
			BIO_set_fp(in, stdin, BIO_NOCLOSE | BIO_FP_TEXT);
		else {
			if (BIO_read_filename(in, x509_config.infile) <= 0) {
				perror(x509_config.infile);
				BIO_free(in);
				goto end;
			}
		}
		req = PEM_read_bio_X509_REQ(in, NULL, NULL, NULL);
		BIO_free(in);

		if (req == NULL) {
			ERR_print_errors(bio_err);
			goto end;
		}
		if ((req->req_info == NULL) ||
		    (req->req_info->pubkey == NULL) ||
		    (req->req_info->pubkey->public_key == NULL) ||
		    (req->req_info->pubkey->public_key->data == NULL)) {
			BIO_printf(bio_err,
			    "The certificate request appears to corrupted\n");
			BIO_printf(bio_err,
			    "It does not contain a public key\n");
			goto end;
		}
		if ((pkey = X509_REQ_get_pubkey(req)) == NULL) {
			BIO_printf(bio_err, "error unpacking public key\n");
			goto end;
		}
		i = X509_REQ_verify(req, pkey);
		EVP_PKEY_free(pkey);
		if (i < 0) {
			BIO_printf(bio_err, "Signature verification error\n");
			ERR_print_errors(bio_err);
			goto end;
		}
		if (i == 0) {
			BIO_printf(bio_err,
			    "Signature did not match the certificate request\n");
			goto end;
		} else
			BIO_printf(bio_err, "Signature ok\n");

		print_name(bio_err, "subject=", X509_REQ_get_subject_name(req),
		    x509_config.nmflag);

		if ((x = X509_new()) == NULL)
			goto end;

		if (x509_config.sno == NULL) {
			x509_config.sno = ASN1_INTEGER_new();
			if (x509_config.sno == NULL ||
			    !rand_serial(NULL, x509_config.sno))
				goto end;
			if (!X509_set_serialNumber(x, x509_config.sno))
				goto end;
			ASN1_INTEGER_free(x509_config.sno);
			x509_config.sno = NULL;
		} else if (!X509_set_serialNumber(x, x509_config.sno))
			goto end;

		if (!X509_set_issuer_name(x, req->req_info->subject))
			goto end;
		if (!X509_set_subject_name(x, req->req_info->subject))
			goto end;

		if (X509_gmtime_adj(X509_get_notBefore(x), 0) == NULL)
			goto end;
		if (X509_time_adj_ex(X509_get_notAfter(x), x509_config.days, 0,
		    NULL) == NULL)
			goto end;

		if ((pkey = X509_REQ_get_pubkey(req)) == NULL)
			goto end;
		if (!X509_set_pubkey(x, pkey)) {
			EVP_PKEY_free(pkey);
			goto end;
		}
		EVP_PKEY_free(pkey);
	} else {
		x = load_cert(bio_err, x509_config.infile, x509_config.informat,
		    NULL, "Certificate");
	}
	if (x == NULL)
		goto end;

	if (x509_config.CA_flag) {
		xca = load_cert(bio_err, x509_config.CAfile,
		    x509_config.CAformat, NULL, "CA Certificate");
		if (xca == NULL)
			goto end;
	}
	if (!x509_config.noout || x509_config.text || x509_config.next_serial) {
		OBJ_create("2.99999.3", "SET.ex3", "SET x509v3 extension 3");

		out = BIO_new(BIO_s_file());
		if (out == NULL) {
			ERR_print_errors(bio_err);
			goto end;
		}
		if (x509_config.outfile == NULL) {
			BIO_set_fp(out, stdout, BIO_NOCLOSE);
		} else {
			if (BIO_write_filename(out, x509_config.outfile) <= 0) {
				perror(x509_config.outfile);
				goto end;
			}
		}
	}
	if (x509_config.alias != NULL) {
		if (!X509_alias_set1(x, (unsigned char *)x509_config.alias, -1))
			goto end;
	}

	if (x509_config.clrtrust)
		X509_trust_clear(x);
	if (x509_config.clrreject)
		X509_reject_clear(x);

	if (x509_config.trust != NULL) {
		for (i = 0; i < sk_ASN1_OBJECT_num(x509_config.trust); i++) {
			x509_config.objtmp = sk_ASN1_OBJECT_value(
			    x509_config.trust, i);
			if (!X509_add1_trust_object(x, x509_config.objtmp))
				goto end;
		}
	}
	if (x509_config.reject != NULL) {
		for (i = 0; i < sk_ASN1_OBJECT_num(x509_config.reject); i++) {
			x509_config.objtmp = sk_ASN1_OBJECT_value(
			    x509_config.reject, i);
			if (!X509_add1_reject_object(x, x509_config.objtmp))
				goto end;
		}
	}
	if (x509_config.num) {
		for (i = 1; i <= x509_config.num; i++) {
			if (x509_config.issuer == i) {
				print_name(STDout, "issuer= ",
				    X509_get_issuer_name(x),
				    x509_config.nmflag);
			} else if (x509_config.subject == i) {
				print_name(STDout, "subject= ",
				    X509_get_subject_name(x),
				    x509_config.nmflag);
			} else if (x509_config.serial == i) {
				BIO_printf(STDout, "serial=");
				i2a_ASN1_INTEGER(STDout,
				    X509_get_serialNumber(x));
				BIO_printf(STDout, "\n");
			} else if (x509_config.next_serial == i) {
				BIGNUM *bnser;
				ASN1_INTEGER *ser;
				ser = X509_get_serialNumber(x);
				if (ser == NULL)
					goto end;
				bnser = ASN1_INTEGER_to_BN(ser, NULL);
				if (bnser == NULL)
					goto end;
				if (!BN_add_word(bnser, 1)) {
					BN_free(bnser);
					goto end;
				}
				ser = BN_to_ASN1_INTEGER(bnser, NULL);
				if (ser == NULL) {
					BN_free(bnser);
					goto end;
				}
				BN_free(bnser);
				i2a_ASN1_INTEGER(out, ser);
				ASN1_INTEGER_free(ser);
				BIO_puts(out, "\n");
			} else if ((x509_config.email == i) ||
			    (x509_config.ocsp_uri == i)) {
				int j;
				STACK_OF(OPENSSL_STRING) *emlst;
				if (x509_config.email == i)
					emlst = X509_get1_email(x);
				else
					emlst = X509_get1_ocsp(x);
				for (j = 0; j < sk_OPENSSL_STRING_num(emlst); j++)
					BIO_printf(STDout, "%s\n",
					    sk_OPENSSL_STRING_value(emlst, j));
				X509_email_free(emlst);
			} else if (x509_config.aliasout == i) {
				unsigned char *alstr;
				alstr = X509_alias_get0(x, NULL);
				if (alstr != NULL)
					BIO_printf(STDout, "%s\n", alstr);
				else
					BIO_puts(STDout, "<No Alias>\n");
			} else if (x509_config.subject_hash == i) {
				BIO_printf(STDout, "%08lx\n",
				    X509_subject_name_hash(x));
			}
#ifndef OPENSSL_NO_MD5
			else if (x509_config.subject_hash_old == i) {
				BIO_printf(STDout, "%08lx\n",
				    X509_subject_name_hash_old(x));
			}
#endif
			else if (x509_config.issuer_hash == i) {
				BIO_printf(STDout, "%08lx\n",
				    X509_issuer_name_hash(x));
			}
#ifndef OPENSSL_NO_MD5
			else if (x509_config.issuer_hash_old == i) {
				BIO_printf(STDout, "%08lx\n",
				    X509_issuer_name_hash_old(x));
			}
#endif
			else if (x509_config.pprint == i) {
				X509_PURPOSE *ptmp;
				int j;
				BIO_printf(STDout, "Certificate purposes:\n");
				for (j = 0; j < X509_PURPOSE_get_count(); j++) {
					ptmp = X509_PURPOSE_get0(j);
					purpose_print(STDout, x, ptmp);
				}
			} else if (x509_config.modulus == i) {
				EVP_PKEY *pkey;

				pkey = X509_get_pubkey(x);
				if (pkey == NULL) {
					BIO_printf(bio_err,
					    "Modulus=unavailable\n");
					ERR_print_errors(bio_err);
					goto end;
				}
				BIO_printf(STDout, "Modulus=");
				if (pkey->type == EVP_PKEY_RSA)
					BN_print(STDout, pkey->pkey.rsa->n);
				else if (pkey->type == EVP_PKEY_DSA)
					BN_print(STDout,
					    pkey->pkey.dsa->pub_key);
				else
					BIO_printf(STDout,
					    "Wrong Algorithm type");
				BIO_printf(STDout, "\n");
				EVP_PKEY_free(pkey);
			} else if (x509_config.pubkey == i) {
				EVP_PKEY *pkey;

				pkey = X509_get_pubkey(x);
				if (pkey == NULL) {
					BIO_printf(bio_err,
					    "Error getting public key\n");
					ERR_print_errors(bio_err);
					goto end;
				}
				PEM_write_bio_PUBKEY(STDout, pkey);
				EVP_PKEY_free(pkey);
			} else if (x509_config.C == i) {
				unsigned char *d;
				char *m;
				int y, z;

				m = X509_NAME_oneline(X509_get_subject_name(x),
				    buf, sizeof buf);
				if (m == NULL)
					goto end;
				BIO_printf(STDout, "/* subject:%s */\n", buf);
				m = X509_NAME_oneline(X509_get_issuer_name(x),
				    buf, sizeof buf);
				if (m == NULL)
					goto end;
				BIO_printf(STDout, "/* issuer :%s */\n", buf);

				z = i2d_X509(x, NULL);
				if (z < 0)
					goto end;

				m = malloc(z);
				if (m == NULL) {
					BIO_printf(bio_err, "out of mem\n");
					goto end;
				}

				d = (unsigned char *) m;
				z = i2d_X509_NAME(X509_get_subject_name(x), &d);
				if (z < 0) {
					free(m);
					goto end;
				}
				BIO_printf(STDout,
				    "unsigned char XXX_subject_name[%d]={\n", z);
				d = (unsigned char *) m;
				for (y = 0; y < z; y++) {
					BIO_printf(STDout, "0x%02X,", d[y]);
					if ((y & 0x0f) == 0x0f)
						BIO_printf(STDout, "\n");
				}
				if (y % 16 != 0)
					BIO_printf(STDout, "\n");
				BIO_printf(STDout, "};\n");

				z = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(x), &d);
				if (z < 0) {
					free(m);
					goto end;
				}
				BIO_printf(STDout,
				    "unsigned char XXX_public_key[%d]={\n", z);
				d = (unsigned char *) m;
				for (y = 0; y < z; y++) {
					BIO_printf(STDout, "0x%02X,", d[y]);
					if ((y & 0x0f) == 0x0f)
						BIO_printf(STDout, "\n");
				}
				if (y % 16 != 0)
					BIO_printf(STDout, "\n");
				BIO_printf(STDout, "};\n");

				z = i2d_X509(x, &d);
				if (z < 0) {
					free(m);
					goto end;
				}
				BIO_printf(STDout,
				    "unsigned char XXX_certificate[%d]={\n", z);
				d = (unsigned char *) m;
				for (y = 0; y < z; y++) {
					BIO_printf(STDout, "0x%02X,", d[y]);
					if ((y & 0x0f) == 0x0f)
						BIO_printf(STDout, "\n");
				}
				if (y % 16 != 0)
					BIO_printf(STDout, "\n");
				BIO_printf(STDout, "};\n");

				free(m);
			} else if (x509_config.text == i) {
				if(!X509_print_ex(STDout, x, x509_config.nmflag,
				    x509_config.certflag))
					goto end;
			} else if (x509_config.startdate == i) {
				ASN1_TIME *nB = X509_get_notBefore(x);
				BIO_puts(STDout, "notBefore=");
				if (ASN1_time_parse(nB->data, nB->length, NULL,
				    0) == -1)
					BIO_puts(STDout,
					    "INVALID RFC5280 TIME");
				else
					ASN1_TIME_print(STDout, nB);
				BIO_puts(STDout, "\n");
			} else if (x509_config.enddate == i) {
				ASN1_TIME *nA = X509_get_notAfter(x);
				BIO_puts(STDout, "notAfter=");
				if (ASN1_time_parse(nA->data, nA->length, NULL,
				    0) == -1)
					BIO_puts(STDout,
					    "INVALID RFC5280 TIME");
				else
					ASN1_TIME_print(STDout, nA);
				BIO_puts(STDout, "\n");
			} else if (x509_config.fingerprint == i) {
				int j;
				unsigned int n;
				unsigned char md[EVP_MAX_MD_SIZE];
				const EVP_MD *fdig = x509_config.digest;

				if (fdig == NULL)
					fdig = EVP_sha256();

				if (!X509_digest(x, fdig, md, &n)) {
					BIO_printf(bio_err, "out of memory\n");
					goto end;
				}
				BIO_printf(STDout, "%s Fingerprint=",
				    OBJ_nid2sn(EVP_MD_type(fdig)));
				for (j = 0; j < (int) n; j++) {
					BIO_printf(STDout, "%02X%c", md[j],
					    (j + 1 == (int)n) ? '\n' : ':');
				}

			/* should be in the library */
			} else if ((x509_config.sign_flag == i) &&
			    (x509_config.x509req == 0)) {
				BIO_printf(bio_err, "Getting Private key\n");
				if (Upkey == NULL) {
					Upkey = load_key(bio_err,
					    x509_config.keyfile,
					    x509_config.keyformat, 0, passin,
					    "Private key");
					if (Upkey == NULL)
						goto end;
				}
				if (!sign(x, Upkey, x509_config.days,
				    x509_config.clrext, x509_config.digest,
				    extconf, x509_config.extsect))
					goto end;
			} else if (x509_config.CA_flag == i) {
				BIO_printf(bio_err, "Getting CA Private Key\n");
				if (x509_config.CAkeyfile != NULL) {
					CApkey = load_key(bio_err,
					    x509_config.CAkeyfile,
					    x509_config.CAkeyformat, 0, passin,
					    "CA Private Key");
					if (CApkey == NULL)
						goto end;
				}
				if (!x509_certify(ctx, x509_config.CAfile,
				    x509_config.digest, x, xca, CApkey,
				    x509_config.sigopts, x509_config.CAserial,
				    x509_config.CA_createserial,
				    x509_config.days, x509_config.clrext,
				    extconf, x509_config.extsect,
				    x509_config.sno))
					goto end;
			} else if (x509_config.x509req == i) {
				EVP_PKEY *pk;

				BIO_printf(bio_err,
				    "Getting request Private Key\n");
				if (x509_config.keyfile == NULL) {
					BIO_printf(bio_err,
					    "no request key file specified\n");
					goto end;
				} else {
					pk = load_key(bio_err,
					    x509_config.keyfile,
					    x509_config.keyformat, 0, passin,
					    "request key");
					if (pk == NULL)
						goto end;
				}

				BIO_printf(bio_err,
				    "Generating certificate request\n");

				rq = X509_to_X509_REQ(x, pk, x509_config.digest);
				EVP_PKEY_free(pk);
				if (rq == NULL) {
					ERR_print_errors(bio_err);
					goto end;
				}
				if (!x509_config.noout) {
					if (!X509_REQ_print(out, rq))
						goto end;
					if (!PEM_write_bio_X509_REQ(out, rq))
						goto end;
				}
				x509_config.noout = 1;
			} else if (x509_config.ocspid == i) {
				if (!X509_ocspid_print(out, x))
					goto end;
			}
		}
	}
	if (x509_config.checkend) {
		time_t tcheck = time(NULL) + x509_config.checkoffset;
		int timecheck = X509_cmp_time(X509_get_notAfter(x), &tcheck);
		if (timecheck == 0) {
			BIO_printf(out, "Certificate expiry time is invalid\n");
			ret = 1;
		} else if (timecheck < 0) {
			BIO_printf(out, "Certificate will expire\n");
			ret = 1;
		} else {
			BIO_printf(out, "Certificate will not expire\n");
			ret = 0;
		}
		goto end;
	}
	if (x509_config.noout) {
		ret = 0;
		goto end;
	}
	if (x509_config.outformat == FORMAT_ASN1)
		i = i2d_X509_bio(out, x);
	else if (x509_config.outformat == FORMAT_PEM) {
		if (x509_config.trustout)
			i = PEM_write_bio_X509_AUX(out, x);
		else
			i = PEM_write_bio_X509(out, x);
	} else if (x509_config.outformat == FORMAT_NETSCAPE) {
		NETSCAPE_X509 nx;
		ASN1_OCTET_STRING hdr;

		hdr.data = (unsigned char *) NETSCAPE_CERT_HDR;
		hdr.length = strlen(NETSCAPE_CERT_HDR);
		nx.header = &hdr;
		nx.cert = x;

		i = ASN1_item_i2d_bio(&NETSCAPE_X509_it, out, &nx);
	} else {
		BIO_printf(bio_err,
		    "bad output format specified for outfile\n");
		goto end;
	}
	if (!i) {
		BIO_printf(bio_err, "unable to write certificate\n");
		ERR_print_errors(bio_err);
		goto end;
	}
	ret = 0;

 end:
	OBJ_cleanup();
	NCONF_free(extconf);
	BIO_free_all(out);
	BIO_free_all(STDout);
	X509_STORE_free(ctx);
	X509_REQ_free(req);
	X509_free(x);
	X509_free(xca);
	EVP_PKEY_free(Upkey);
	EVP_PKEY_free(CApkey);
	sk_OPENSSL_STRING_free(x509_config.sigopts);
	X509_REQ_free(rq);
	ASN1_INTEGER_free(x509_config.sno);
	sk_ASN1_OBJECT_pop_free(x509_config.trust, ASN1_OBJECT_free);
	sk_ASN1_OBJECT_pop_free(x509_config.reject, ASN1_OBJECT_free);
	free(passin);

	return (ret);
}

static ASN1_INTEGER *
x509_load_serial(char *CAfile, char *serialfile, int create)
{
	char *buf = NULL, *p;
	ASN1_INTEGER *bs = NULL;
	BIGNUM *serial = NULL;
	size_t len;

	len = ((serialfile == NULL) ? (strlen(CAfile) + strlen(POSTFIX) + 1) :
	    (strlen(serialfile))) + 1;
	buf = malloc(len);
	if (buf == NULL) {
		BIO_printf(bio_err, "out of mem\n");
		goto end;
	}
	if (serialfile == NULL) {
		strlcpy(buf, CAfile, len);
		for (p = buf; *p; p++)
			if (*p == '.') {
				*p = '\0';
				break;
			}
		strlcat(buf, POSTFIX, len);
	} else
		strlcpy(buf, serialfile, len);

	serial = load_serial(buf, create, NULL);
	if (serial == NULL)
		goto end;

	if (!BN_add_word(serial, 1)) {
		BIO_printf(bio_err, "add_word failure\n");
		goto end;
	}
	if (!save_serial(buf, NULL, serial, &bs))
		goto end;

 end:
	free(buf);
	BN_free(serial);

	return bs;
}

static int
x509_certify(X509_STORE *ctx, char *CAfile, const EVP_MD *digest, X509 *x,
    X509 *xca, EVP_PKEY *pkey, STACK_OF(OPENSSL_STRING) *sigopts,
    char *serialfile, int create, int days, int clrext, CONF *conf,
    char *section, ASN1_INTEGER *sno)
{
	int ret = 0;
	ASN1_INTEGER *bs = NULL;
	X509_STORE_CTX xsc;
	EVP_PKEY *upkey;

	upkey = X509_get_pubkey(xca);
	if (upkey == NULL)
		goto end;
	EVP_PKEY_copy_parameters(upkey, pkey);
	EVP_PKEY_free(upkey);

	if (!X509_STORE_CTX_init(&xsc, ctx, x, NULL)) {
		BIO_printf(bio_err, "Error initialising X509 store\n");
		goto end;
	}
	if (sno != NULL)
		bs = sno;
	else if ((bs = x509_load_serial(CAfile, serialfile, create)) == NULL)
		goto end;

/*	if (!X509_STORE_add_cert(ctx,x)) goto end;*/

	/*
	 * NOTE: this certificate can/should be self signed, unless it was a
	 * certificate request in which case it is not.
	 */
	X509_STORE_CTX_set_cert(&xsc, x);
	X509_STORE_CTX_set_flags(&xsc, X509_V_FLAG_CHECK_SS_SIGNATURE);
	if (!x509_config.reqfile && X509_verify_cert(&xsc) <= 0)
		goto end;

	if (!X509_check_private_key(xca, pkey)) {
		BIO_printf(bio_err,
		    "CA certificate and CA private key do not match\n");
		goto end;
	}
	if (!X509_set_issuer_name(x, X509_get_subject_name(xca)))
		goto end;
	if (!X509_set_serialNumber(x, bs))
		goto end;

	if (X509_gmtime_adj(X509_get_notBefore(x), 0L) == NULL)
		goto end;

	/* hardwired expired */
	if (X509_time_adj_ex(X509_get_notAfter(x), days, 0, NULL) == NULL)
		goto end;

	if (clrext) {
		while (X509_get_ext_count(x) > 0) {
			if (X509_delete_ext(x, 0) == NULL)
				goto end;
		}
	}
	if (conf != NULL) {
		X509V3_CTX ctx2;
		if (!X509_set_version(x, 2))	/* version 3 certificate */
			goto end;
		X509V3_set_ctx(&ctx2, xca, x, NULL, NULL, 0);
		X509V3_set_nconf(&ctx2, conf);
		if (!X509V3_EXT_add_nconf(conf, &ctx2, section, x))
			goto end;
	}
	if (!do_X509_sign(bio_err, x, pkey, digest, sigopts))
		goto end;

	ret = 1;
 end:
	X509_STORE_CTX_cleanup(&xsc);
	if (!ret)
		ERR_print_errors(bio_err);
	if (sno == NULL)
		ASN1_INTEGER_free(bs);
	return ret;
}

static int
callb(int ok, X509_STORE_CTX *ctx)
{
	int err;
	X509 *err_cert;

	/*
	 * it is ok to use a self signed certificate This case will catch
	 * both the initial ok == 0 and the final ok == 1 calls to this
	 * function
	 */
	err = X509_STORE_CTX_get_error(ctx);
	if (err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT)
		return 1;

	/*
	 * BAD we should have gotten an error.  Normally if everything worked
	 * X509_STORE_CTX_get_error(ctx) will still be set to
	 * DEPTH_ZERO_SELF_....
	 */
	if (ok) {
		BIO_printf(bio_err,
		    "error with certificate to be certified - should be self signed\n");
		return 0;
	} else {
		err_cert = X509_STORE_CTX_get_current_cert(ctx);
		print_name(bio_err, NULL, X509_get_subject_name(err_cert), 0);
		BIO_printf(bio_err,
		    "error with certificate - error %d at depth %d\n%s\n",
		    err, X509_STORE_CTX_get_error_depth(ctx),
		    X509_verify_cert_error_string(err));
		return 1;
	}
}

/* self sign */
static int
sign(X509 *x, EVP_PKEY *pkey, int days, int clrext, const EVP_MD *digest,
    CONF *conf, char *section)
{
	EVP_PKEY *pktmp;

	pktmp = X509_get_pubkey(x);
	if (pktmp == NULL)
		goto err;
	EVP_PKEY_copy_parameters(pktmp, pkey);
	EVP_PKEY_save_parameters(pktmp, 1);
	EVP_PKEY_free(pktmp);

	if (!X509_set_issuer_name(x, X509_get_subject_name(x)))
		goto err;
	if (X509_gmtime_adj(X509_get_notBefore(x), 0) == NULL)
		goto err;

	/* Lets just make it 12:00am GMT, Jan 1 1970 */
	/* memcpy(x->cert_info->validity->notBefore,"700101120000Z",13); */
	/* 28 days to be certified */

	if (X509_gmtime_adj(X509_get_notAfter(x),
	    (long) 60 * 60 * 24 * days) == NULL)
		goto err;

	if (!X509_set_pubkey(x, pkey))
		goto err;
	if (clrext) {
		while (X509_get_ext_count(x) > 0) {
			if (X509_delete_ext(x, 0) == NULL)
				goto err;
		}
	}
	if (conf != NULL) {
		X509V3_CTX ctx;
		if (!X509_set_version(x, 2))	/* version 3 certificate */
			goto err;
		X509V3_set_ctx(&ctx, x, x, NULL, NULL, 0);
		X509V3_set_nconf(&ctx, conf);
		if (!X509V3_EXT_add_nconf(conf, &ctx, section, x))
			goto err;
	}
	if (!X509_sign(x, pkey, digest))
		goto err;

	return 1;

 err:
	ERR_print_errors(bio_err);
	return 0;
}

static int
purpose_print(BIO *bio, X509 *cert, X509_PURPOSE *pt)
{
	int id, i, idret;
	char *pname;

	id = X509_PURPOSE_get_id(pt);
	pname = X509_PURPOSE_get0_name(pt);
	for (i = 0; i < 2; i++) {
		idret = X509_check_purpose(cert, id, i);
		BIO_printf(bio, "%s%s : ", pname, i ? " CA" : "");
		if (idret == 1)
			BIO_printf(bio, "Yes\n");
		else if (idret == 0)
			BIO_printf(bio, "No\n");
		else
			BIO_printf(bio, "Yes (WARNING code=%d)\n", idret);
	}
	return 1;
}
