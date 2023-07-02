/* $OpenBSD: req.c,v 1.28 2023/07/02 07:05:14 tb Exp $ */
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

/* Until the key-gen callbacks are modified to use newer prototypes, we allow
 * deprecated functions for openssl-internal code */
#ifdef OPENSSL_NO_DEPRECATED
#undef OPENSSL_NO_DEPRECATED
#endif

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "apps.h"

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <openssl/dsa.h>

#include <openssl/rsa.h>

#define SECTION		"req"

#define BITS		"default_bits"
#define KEYFILE		"default_keyfile"
#define PROMPT		"prompt"
#define DISTINGUISHED_NAME	"distinguished_name"
#define ATTRIBUTES	"attributes"
#define V3_EXTENSIONS	"x509_extensions"
#define REQ_EXTENSIONS	"req_extensions"
#define STRING_MASK	"string_mask"
#define UTF8_IN		"utf8"

#define DEFAULT_KEY_LENGTH	2048
#define MIN_KEY_LENGTH		384

static int make_REQ(X509_REQ * req, EVP_PKEY * pkey, char *dn, int multirdn,
    int attribs, unsigned long chtype);
static int build_subject(X509_REQ * req, char *subj, unsigned long chtype,
    int multirdn);
static int prompt_info(X509_REQ * req,
    STACK_OF(CONF_VALUE) * dn_sk, char *dn_sect,
    STACK_OF(CONF_VALUE) * attr_sk, char *attr_sect, int attribs,
    unsigned long chtype);
static int auto_info(X509_REQ * req, STACK_OF(CONF_VALUE) * sk,
    STACK_OF(CONF_VALUE) * attr, int attribs,
    unsigned long chtype);
static int add_attribute_object(X509_REQ * req, char *text, const char *def,
    char *value, int nid, int n_min,
    int n_max, unsigned long chtype);
static int add_DN_object(X509_NAME * n, char *text, const char *def, char *value,
    int nid, int n_min, int n_max, unsigned long chtype, int mval);
static int genpkey_cb(EVP_PKEY_CTX * ctx);
static int req_check_len(int len, int n_min, int n_max);
static int check_end(const char *str, const char *end);
static EVP_PKEY_CTX *set_keygen_ctx(BIO * err, const char *gstr, int *pkey_type,
    long *pkeylen, char **palgnam);
static unsigned long ext_name_hash(const OPENSSL_STRING *a);
static int ext_name_cmp(const OPENSSL_STRING *a, const OPENSSL_STRING *b);
static void exts_cleanup(OPENSSL_STRING *x);
static int duplicated(LHASH_OF(OPENSSL_STRING) *addexts, char *kv);
static CONF *req_conf = NULL;
static CONF *addext_conf = NULL;

static struct {
	LHASH_OF(OPENSSL_STRING) *addexts;
	BIO *addext_bio;
	int batch;
	unsigned long chtype;
	int days;
	const EVP_MD *digest;
	char *extensions;
	char *infile;
	int informat;
	char *keyalg;
	char *keyfile;
	int keyform;
	char *keyout;
	int modulus;
	int multirdn;
	int newhdr;
	long newkey;
	int newreq;
	unsigned long nmflag;
	int nodes;
	int noout;
	char *outfile;
	int outformat;
	char *passargin;
	char *passargout;
	STACK_OF(OPENSSL_STRING) *pkeyopts;
	int pubkey;
	char *req_exts;
	unsigned long reqflag;
	ASN1_INTEGER *serial;
	STACK_OF(OPENSSL_STRING) *sigopts;
	char *subj;
	int subject;
	char *template;
	int text;
	int verbose;
	int verify;
	int x509;
} cfg;

static int
req_opt_addext(char *arg)
{
	int i;

	if (cfg.addexts == NULL) {
		cfg.addexts = (LHASH_OF(OPENSSL_STRING) *)lh_new(
		    (LHASH_HASH_FN_TYPE)ext_name_hash,
		    (LHASH_COMP_FN_TYPE)ext_name_cmp);
		cfg.addext_bio = BIO_new(BIO_s_mem());
		if (cfg.addexts == NULL ||
		    cfg.addext_bio == NULL)
			return (1);
	}
	i = duplicated(cfg.addexts, arg);
	if (i == 1)
		return (1);
	if (i < 0 || BIO_printf(cfg.addext_bio, "%s\n", arg) < 0)
		return (1);

	return (0);
}

static int
req_opt_days(char *arg)
{
	const char *errstr;

	cfg.days = strtonum(arg, 1, INT_MAX, &errstr);
	if (errstr != NULL) {
		BIO_printf(bio_err, "bad -days %s, using 0: %s\n",
		    arg, errstr);
		cfg.days = 30;
	}
	return (0);
}

static int
req_opt_digest(int argc, char **argv, int *argsused)
{
	char *name = argv[0];

	if (*name++ != '-')
		return (1);

	if ((cfg.digest = EVP_get_digestbyname(name)) == NULL)
		return (1);

	*argsused = 1;
	return (0);
}

static int
req_opt_newkey(char *arg)
{
	cfg.keyalg = arg;
	cfg.newreq = 1;
	return (0);
}

static int
req_opt_nameopt(char *arg)
{
	if (!set_name_ex(&cfg.nmflag, arg))
		return (1);
	return (0);
}

static int
req_opt_pkeyopt(char *arg)
{
	if (cfg.pkeyopts == NULL)
		cfg.pkeyopts = sk_OPENSSL_STRING_new_null();
	if (cfg.pkeyopts == NULL)
		return (1);
	if (!sk_OPENSSL_STRING_push(cfg.pkeyopts, arg))
		return (1);
	return (0);
}

static int
req_opt_reqopt(char *arg)
{
	if (!set_cert_ex(&cfg.reqflag, arg))
		return (1);
	return (0);
}

static int
req_opt_set_serial(char *arg)
{
	cfg.serial = s2i_ASN1_INTEGER(NULL, arg);
	if (cfg.serial == NULL)
		return (1);
	return (0);
}

static int
req_opt_sigopt(char *arg)
{
	if (cfg.sigopts == NULL)
		cfg.sigopts = sk_OPENSSL_STRING_new_null();
	if (cfg.sigopts == NULL)
		return (1);
	if (!sk_OPENSSL_STRING_push(cfg.sigopts, arg))
		return (1);
	return (0);
}

static int
req_opt_utf8(void)
{
	cfg.chtype = MBSTRING_UTF8;
	return (0);
}

static const struct option req_options[] = {
	{
		.name = "addext",
		.argname = "key=value",
		.desc = "Additional certificate extension (may be repeated)",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = req_opt_addext,
	},
	{
		.name = "batch",
		.desc = "Operate in batch mode",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.batch,
	},
	{
		.name = "config",
		.argname = "file",
		.desc = "Configuration file to use as request template",
		.type = OPTION_ARG,
		.opt.arg = &cfg.template,
	},
	{
		.name = "days",
		.argname = "number",
		.desc = "Number of days generated certificate is valid for",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = req_opt_days,
	},
	{
		.name = "extensions",
		.argname = "section",
		.desc = "Config section to use for certificate extensions",
		.type = OPTION_ARG,
		.opt.arg = &cfg.extensions,
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
		.name = "key",
		.argname = "file",
		.desc = "Private key file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.keyfile,
	},
	{
		.name = "keyform",
		.argname = "format",
		.desc = "Private key format (DER or PEM (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.keyform,
	},
	{
		.name = "keyout",
		.argname = "file",
		.desc = "Private key output file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.keyout,
	},
	{
		.name = "modulus",
		.desc = "Print RSA modulus",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.modulus,
	},
	{
		.name = "multivalue-rdn",
		.desc = "Enable support for multivalued RDNs",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.multirdn,
	},
	{
		.name = "nameopt",
		.argname = "arg",
		.desc = "Certificate name options",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = req_opt_nameopt,
	},
	{
		.name = "new",
		.desc = "New request",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.newreq,
	},
	{
		.name = "newhdr",
		.desc = "Include 'NEW' in header lines",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.newhdr,
	},
	{
		.name = "newkey",
		.argname = "param",
		.desc = "Generate a new key using given parameters",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = req_opt_newkey,
	},
	{
		.name = "nodes",
		.desc = "Do not encrypt output private key",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.nodes,
	},
	{
		.name = "noout",
		.desc = "Do not output request",
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
		.name = "passin",
		.argname = "source",
		.desc = "Private key input password source",
		.type = OPTION_ARG,
		.opt.arg = &cfg.passargin,
	},
	{
		.name = "passout",
		.argname = "source",
		.desc = "Private key output password source",
		.type = OPTION_ARG,
		.opt.arg = &cfg.passargout,
	},
	{
		.name = "pkeyopt",
		.argname = "opt:val",
		.desc = "Set the public key algorithm option opt to val",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = req_opt_pkeyopt,
	},
	{
		.name = "pubkey",
		.desc = "Output the public key",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.pubkey,
	},
	{
		.name = "reqexts",
		.argname = "section",
		.desc = "Config section to use for request extensions",
		.type = OPTION_ARG,
		.opt.arg = &cfg.req_exts,
	},
	{
		.name = "reqopt",
		.argname = "option",
		.desc = "Request text options",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = req_opt_reqopt,
	},
	{
		.name = "set_serial",
		.argname = "serial",
		.desc = "Serial number to use for generated certificate",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = req_opt_set_serial,
	},
	{
		.name = "sigopt",
		.argname = "name:val",
		.desc = "Signature options",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = req_opt_sigopt,
	},
	{
		.name = "subj",
		.argname = "name",
		.desc = "Set or modify the request subject",
		.type = OPTION_ARG,
		.opt.arg = &cfg.subj,
	},
	{
		.name = "subject",
		.desc = "Output the subject of the request",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.subject,
	},
	{
		.name = "text",
		.desc = "Print request in text form",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.text,
	},
	{
		.name = "utf8",
		.desc = "Input characters are in UTF-8 (default ASCII)",
		.type = OPTION_FUNC,
		.opt.func = req_opt_utf8,
	},
	{
		.name = "verbose",
		.desc = "Verbose",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.verbose,
	},
	{
		.name = "verify",
		.desc = "Verify signature on request",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.verify,
	},
	{
		.name = "x509",
		.desc = "Output an X.509 structure instead of a certificate request",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.x509,
	},
	{
		.name = NULL,
		.desc = "",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = req_opt_digest,
	},
	{ NULL },
};

static void
req_usage(void)
{
	fprintf(stderr,
	    "usage: req [-addext ext] [-batch] [-config file]\n"
	    "    [-days n] [-extensions section] [-in file]\n"
	    "    [-inform der | pem] [-key keyfile] [-keyform der | pem]\n"
	    "    [-keyout file] [-md4 | -md5 | -sha1] [-modulus]\n"
	    "    [-multivalue-rdn] [-nameopt option] [-new] [-newhdr]\n"
	    "    [-newkey arg] [-nodes] [-noout]\n"
	    "    [-out file] [-outform der | pem] [-passin arg]\n"
	    "    [-passout arg] [-pkeyopt opt:value] [-pubkey]\n"
	    "    [-reqexts section] [-reqopt option] [-set_serial n]\n"
	    "    [-sigopt nm:v] [-subj arg] [-subject] [-text] [-utf8]\n"
	    "    [-verbose] [-verify] [-x509]\n\n");

	options_usage(req_options);
	fprintf(stderr, "\n");
}

int
req_main(int argc, char **argv)
{
	int ex = 1;
	X509 *x509ss = NULL;
	X509_REQ *req = NULL;
	EVP_PKEY_CTX *genctx = NULL;
	char *keyalgstr = NULL;
	const EVP_CIPHER *cipher = NULL;
	EVP_PKEY *pkey = NULL;
	int i = 0, pkey_type = -1;
	BIO *in = NULL, *out = NULL;
	char *passin = NULL, *passout = NULL;
	const EVP_MD *md_alg = NULL;
	char *p;

	if (pledge("stdio cpath wpath rpath tty", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	memset(&cfg, 0, sizeof(cfg));

	cfg.chtype = MBSTRING_ASC;
	cfg.days = 30;
	cfg.digest = EVP_sha256();
	cfg.newkey = -1;
	cfg.informat = FORMAT_PEM;
	cfg.keyform = FORMAT_PEM;
	cfg.outformat = FORMAT_PEM;

	if (options_parse(argc, argv, req_options, NULL, NULL) != 0) {
		req_usage();
		return (1);
	}

	req_conf = NULL;
	cipher = EVP_aes_256_cbc();

	if (!app_passwd(bio_err, cfg.passargin, cfg.passargout, &passin, &passout)) {
		BIO_printf(bio_err, "Error getting passwords\n");
		goto end;
	}
	if (cfg.template != NULL) {
		long errline = -1;

		if (cfg.verbose)
			BIO_printf(bio_err, "Using configuration from %s\n", cfg.template);
		if ((req_conf = NCONF_new(NULL)) == NULL)
			goto end;
		if(!NCONF_load(req_conf, cfg.template, &errline)) {
			BIO_printf(bio_err, "error on line %ld of %s\n", errline, cfg.template);
			goto end;
		}
	} else {
		req_conf = config;

		if (req_conf == NULL) {
			BIO_printf(bio_err, "Unable to load config info from %s\n", default_config_file);
			if (cfg.newreq)
				goto end;
		} else if (cfg.verbose)
			BIO_printf(bio_err, "Using configuration from %s\n",
			    default_config_file);
	}

	if (cfg.addext_bio != NULL) {
		long errline = -1;
		if (cfg.verbose)
			BIO_printf(bio_err,
			    "Using additional configuration from command line\n");
		if ((addext_conf = NCONF_new(NULL)) == NULL)
			goto end;
		if (!NCONF_load_bio(addext_conf, cfg.addext_bio, &errline)) {
			BIO_printf(bio_err,
			    "req: Error on line %ld of config input\n",
			    errline);
			goto end;
		}
	}

	if (req_conf != NULL) {
		if (!load_config(bio_err, req_conf))
			goto end;
		p = NCONF_get_string(req_conf, NULL, "oid_file");
		if (p == NULL)
			ERR_clear_error();
		if (p != NULL) {
			BIO *oid_bio;

			oid_bio = BIO_new_file(p, "r");
			if (oid_bio == NULL) {
				/*
				BIO_printf(bio_err,"problems opening %s for extra oid's\n",p);
				ERR_print_errors(bio_err);
				*/
			} else {
				OBJ_create_objects(oid_bio);
				BIO_free(oid_bio);
			}
		}
	}
	if (!add_oid_section(bio_err, req_conf))
		goto end;

	if (md_alg == NULL) {
		p = NCONF_get_string(req_conf, SECTION, "default_md");
		if (p == NULL)
			ERR_clear_error();
		if (p != NULL) {
			if ((md_alg = EVP_get_digestbyname(p)) != NULL)
				cfg.digest = md_alg;
		}
	}
	if (!cfg.extensions) {
		cfg.extensions = NCONF_get_string(req_conf, SECTION, V3_EXTENSIONS);
		if (!cfg.extensions)
			ERR_clear_error();
	}
	if (cfg.extensions) {
		/* Check syntax of file */
		X509V3_CTX ctx;
		X509V3_set_ctx_test(&ctx);
		X509V3_set_nconf(&ctx, req_conf);
		if (!X509V3_EXT_add_nconf(req_conf, &ctx, cfg.extensions, NULL)) {
			BIO_printf(bio_err,
			    "Error Loading extension section %s\n", cfg.extensions);
			goto end;
		}
	}
	if (addext_conf != NULL) {
		/* Check syntax of command line extensions */
		X509V3_CTX ctx;
		X509V3_set_ctx_test(&ctx);
		X509V3_set_nconf(&ctx, addext_conf);
		if (!X509V3_EXT_add_nconf(addext_conf, &ctx, "default", NULL)) {
			BIO_printf(bio_err,
			    "Error Loading command line extensions\n");
			goto end;
		}
	}
	if (!passin) {
		passin = NCONF_get_string(req_conf, SECTION, "input_password");
		if (!passin)
			ERR_clear_error();
	}
	if (!passout) {
		passout = NCONF_get_string(req_conf, SECTION, "output_password");
		if (!passout)
			ERR_clear_error();
	}
	p = NCONF_get_string(req_conf, SECTION, STRING_MASK);
	if (!p)
		ERR_clear_error();

	if (p && !ASN1_STRING_set_default_mask_asc(p)) {
		BIO_printf(bio_err, "Invalid global string mask setting %s\n", p);
		goto end;
	}
	if (cfg.chtype != MBSTRING_UTF8) {
		p = NCONF_get_string(req_conf, SECTION, UTF8_IN);
		if (!p)
			ERR_clear_error();
		else if (!strcmp(p, "yes"))
			cfg.chtype = MBSTRING_UTF8;
	}
	if (!cfg.req_exts) {
		cfg.req_exts = NCONF_get_string(req_conf, SECTION, REQ_EXTENSIONS);
		if (!cfg.req_exts)
			ERR_clear_error();
	}
	if (cfg.req_exts) {
		/* Check syntax of file */
		X509V3_CTX ctx;
		X509V3_set_ctx_test(&ctx);
		X509V3_set_nconf(&ctx, req_conf);
		if (!X509V3_EXT_add_nconf(req_conf, &ctx, cfg.req_exts, NULL)) {
			BIO_printf(bio_err,
			    "Error Loading request extension section %s\n",
			    cfg.req_exts);
			goto end;
		}
	}
	in = BIO_new(BIO_s_file());
	out = BIO_new(BIO_s_file());
	if ((in == NULL) || (out == NULL))
		goto end;

	if (cfg.keyfile != NULL) {
		pkey = load_key(bio_err, cfg.keyfile, cfg.keyform, 0, passin,
		    "Private Key");
		if (!pkey) {
			/*
			 * load_key() has already printed an appropriate
			 * message
			 */
			goto end;
		}
	}
	if (cfg.newreq && (pkey == NULL)) {
		if (!NCONF_get_number(req_conf, SECTION, BITS, &cfg.newkey)) {
			cfg.newkey = DEFAULT_KEY_LENGTH;
		}
		if (cfg.keyalg) {
			genctx = set_keygen_ctx(bio_err, cfg.keyalg, &pkey_type, &cfg.newkey,
			    &keyalgstr);
			if (!genctx)
				goto end;
		}
		if (cfg.newkey < MIN_KEY_LENGTH && (pkey_type == EVP_PKEY_RSA || pkey_type == EVP_PKEY_DSA)) {
			BIO_printf(bio_err, "private key length is too short,\n");
			BIO_printf(bio_err, "it needs to be at least %d bits, not %ld\n", MIN_KEY_LENGTH, cfg.newkey);
			goto end;
		}
		if (!genctx) {
			genctx = set_keygen_ctx(bio_err, NULL, &pkey_type, &cfg.newkey,
			    &keyalgstr);
			if (!genctx)
				goto end;
		}
		if (cfg.pkeyopts) {
			char *genopt;
			for (i = 0; i < sk_OPENSSL_STRING_num(cfg.pkeyopts); i++) {
				genopt = sk_OPENSSL_STRING_value(cfg.pkeyopts, i);
				if (pkey_ctrl_string(genctx, genopt) <= 0) {
					BIO_printf(bio_err,
					    "parameter error \"%s\"\n",
					    genopt);
					ERR_print_errors(bio_err);
					goto end;
				}
			}
		}
		BIO_printf(bio_err, "Generating a %ld bit %s private key\n",
		    cfg.newkey, keyalgstr);

		EVP_PKEY_CTX_set_cb(genctx, genpkey_cb);
		EVP_PKEY_CTX_set_app_data(genctx, bio_err);

		if (EVP_PKEY_keygen(genctx, &pkey) <= 0) {
			BIO_puts(bio_err, "Error Generating Key\n");
			goto end;
		}
		EVP_PKEY_CTX_free(genctx);
		genctx = NULL;

		if (cfg.keyout == NULL) {
			cfg.keyout = NCONF_get_string(req_conf, SECTION, KEYFILE);
			if (cfg.keyout == NULL)
				ERR_clear_error();
		}
		if (cfg.keyout == NULL) {
			BIO_printf(bio_err, "writing new private key to stdout\n");
			BIO_set_fp(out, stdout, BIO_NOCLOSE);
		} else {
			BIO_printf(bio_err, "writing new private key to '%s'\n", cfg.keyout);
			if (BIO_write_filename(out, cfg.keyout) <= 0) {
				perror(cfg.keyout);
				goto end;
			}
		}

		p = NCONF_get_string(req_conf, SECTION, "encrypt_rsa_key");
		if (p == NULL) {
			ERR_clear_error();
			p = NCONF_get_string(req_conf, SECTION, "encrypt_key");
			if (p == NULL)
				ERR_clear_error();
		}
		if ((p != NULL) && (strcmp(p, "no") == 0))
			cipher = NULL;
		if (cfg.nodes)
			cipher = NULL;

		i = 0;
 loop:
		if (!PEM_write_bio_PrivateKey(out, pkey, cipher,
			NULL, 0, NULL, passout)) {
			if ((ERR_GET_REASON(ERR_peek_error()) ==
				PEM_R_PROBLEMS_GETTING_PASSWORD) && (i < 3)) {
				ERR_clear_error();
				i++;
				goto loop;
			}
			goto end;
		}
		BIO_printf(bio_err, "-----\n");
	}
	if (!cfg.newreq) {
		if (cfg.infile == NULL)
			BIO_set_fp(in, stdin, BIO_NOCLOSE);
		else {
			if (BIO_read_filename(in, cfg.infile) <= 0) {
				perror(cfg.infile);
				goto end;
			}
		}

		if (cfg.informat == FORMAT_ASN1)
			req = d2i_X509_REQ_bio(in, NULL);
		else if (cfg.informat == FORMAT_PEM)
			req = PEM_read_bio_X509_REQ(in, NULL, NULL, NULL);
		else {
			BIO_printf(bio_err, "bad input format specified for X509 request\n");
			goto end;
		}
		if (req == NULL) {
			BIO_printf(bio_err, "unable to load X509 request\n");
			goto end;
		}
	}
	if (cfg.newreq || cfg.x509) {
		if (pkey == NULL) {
			BIO_printf(bio_err, "you need to specify a private key\n");
			goto end;
		}
		if (req == NULL) {
			req = X509_REQ_new();
			if (req == NULL) {
				goto end;
			}
			i = make_REQ(req, pkey, cfg.subj, cfg.multirdn, !cfg.x509, cfg.chtype);
			cfg.subj = NULL;	/* done processing '-subj' option */
			if (!i) {
				BIO_printf(bio_err, "problems making Certificate Request\n");
				goto end;
			}
		}
		if (cfg.x509) {
			EVP_PKEY *tmppkey;

			X509V3_CTX ext_ctx;
			if ((x509ss = X509_new()) == NULL)
				goto end;

			/* Set version to V3 */
			if ((cfg.extensions != NULL || addext_conf != NULL) &&
			    !X509_set_version(x509ss, 2))
				goto end;
			if (cfg.serial) {
				if (!X509_set_serialNumber(x509ss, cfg.serial))
					goto end;
			} else {
				if (!rand_serial(NULL,
					X509_get_serialNumber(x509ss)))
					goto end;
			}

			if (!X509_set_issuer_name(x509ss, X509_REQ_get_subject_name(req)))
				goto end;
			if (!X509_gmtime_adj(X509_get_notBefore(x509ss), 0))
				goto end;
			if (!X509_time_adj_ex(X509_get_notAfter(x509ss), cfg.days, 0, NULL))
				goto end;
			if (!X509_set_subject_name(x509ss, X509_REQ_get_subject_name(req)))
				goto end;
			if ((tmppkey = X509_REQ_get0_pubkey(req)) == NULL)
				goto end;
			if (!X509_set_pubkey(x509ss, tmppkey))
				goto end;

			/* Set up V3 context struct */

			X509V3_set_ctx(&ext_ctx, x509ss, x509ss, NULL, NULL, 0);
			X509V3_set_nconf(&ext_ctx, req_conf);

			/* Add extensions */
			if (cfg.extensions && !X509V3_EXT_add_nconf(req_conf,
				&ext_ctx, cfg.extensions, x509ss)) {
				BIO_printf(bio_err,
				    "Error Loading extension section %s\n",
				    cfg.extensions);
				goto end;
			}
			if (addext_conf != NULL &&
			    !X509V3_EXT_add_nconf(addext_conf, &ext_ctx,
				    "default", x509ss)) {
				BIO_printf(bio_err,
				    "Error Loading command line extensions\n");
				goto end;
			}
			i = do_X509_sign(bio_err, x509ss, pkey, cfg.digest, cfg.sigopts);
			if (!i) {
				ERR_print_errors(bio_err);
				goto end;
			}
		} else {
			X509V3_CTX ext_ctx;

			/* Set up V3 context struct */

			X509V3_set_ctx(&ext_ctx, NULL, NULL, req, NULL, 0);
			X509V3_set_nconf(&ext_ctx, req_conf);

			/* Add extensions */
			if (cfg.req_exts && !X509V3_EXT_REQ_add_nconf(req_conf,
				&ext_ctx, cfg.req_exts, req)) {
				BIO_printf(bio_err,
				    "Error Loading extension section %s\n",
				    cfg.req_exts);
				goto end;
			}
			if (addext_conf != NULL &&
			    !X509V3_EXT_REQ_add_nconf(addext_conf, &ext_ctx,
				    "default", req)) {
				BIO_printf(bio_err,
				    "Error Loading command line extensions\n");
				goto end;
			}
			i = do_X509_REQ_sign(bio_err, req, pkey, cfg.digest, cfg.sigopts);
			if (!i) {
				ERR_print_errors(bio_err);
				goto end;
			}
		}
	}
	if (cfg.subj && cfg.x509) {
		BIO_printf(bio_err, "Cannot modify certificate subject\n");
		goto end;
	}
	if (cfg.subj && !cfg.x509) {
		if (cfg.verbose) {
			BIO_printf(bio_err, "Modifying Request's Subject\n");
			print_name(bio_err, "old subject=", X509_REQ_get_subject_name(req), cfg.nmflag);
		}
		if (build_subject(req, cfg.subj, cfg.chtype, cfg.multirdn) == 0) {
			BIO_printf(bio_err, "ERROR: cannot modify subject\n");
			ex = 1;
			goto end;
		}

		if (cfg.verbose) {
			print_name(bio_err, "new subject=", X509_REQ_get_subject_name(req), cfg.nmflag);
		}
	}
	if (cfg.verify && !cfg.x509) {
		EVP_PKEY *pubkey = pkey;

		if (pubkey == NULL)
			pubkey = X509_REQ_get0_pubkey(req);
		if (pubkey == NULL)
			goto end;
		i = X509_REQ_verify(req, pubkey);
		if (i < 0) {
			goto end;
		} else if (i == 0) {
			BIO_printf(bio_err, "verify failure\n");
			ERR_print_errors(bio_err);
		} else		/* if (i > 0) */
			BIO_printf(bio_err, "verify OK\n");
	}
	if (cfg.noout && !cfg.text && !cfg.modulus && !cfg.subject && !cfg.pubkey) {
		ex = 0;
		goto end;
	}
	if (cfg.outfile == NULL) {
		BIO_set_fp(out, stdout, BIO_NOCLOSE);
	} else {
		if ((cfg.keyout != NULL) && (strcmp(cfg.outfile, cfg.keyout) == 0))
			i = (int) BIO_append_filename(out, cfg.outfile);
		else
			i = (int) BIO_write_filename(out, cfg.outfile);
		if (!i) {
			perror(cfg.outfile);
			goto end;
		}
	}

	if (cfg.pubkey) {
		EVP_PKEY *tpubkey;

		if ((tpubkey = X509_REQ_get0_pubkey(req)) == NULL) {
			BIO_printf(bio_err, "Error getting public key\n");
			ERR_print_errors(bio_err);
			goto end;
		}
		PEM_write_bio_PUBKEY(out, tpubkey);
	}
	if (cfg.text) {
		if (cfg.x509)
			X509_print_ex(out, x509ss, cfg.nmflag, cfg.reqflag);
		else
			X509_REQ_print_ex(out, req, cfg.nmflag, cfg.reqflag);
	}
	if (cfg.subject) {
		if (cfg.x509)
			print_name(out, "subject=", X509_get_subject_name(x509ss), cfg.nmflag);
		else
			print_name(out, "subject=", X509_REQ_get_subject_name(req), cfg.nmflag);
	}
	if (cfg.modulus) {
		EVP_PKEY *tpubkey;

		if (cfg.x509)
			tpubkey = X509_get0_pubkey(x509ss);
		else
			tpubkey = X509_REQ_get0_pubkey(req);
		if (tpubkey == NULL) {
			fprintf(stdout, "Modulus=unavailable\n");
			goto end;
		}
		fprintf(stdout, "Modulus=");
		if (EVP_PKEY_base_id(tpubkey) == EVP_PKEY_RSA) {
			const BIGNUM *n = NULL;

			RSA_get0_key(EVP_PKEY_get0_RSA(tpubkey), &n, NULL, NULL);

			BN_print(out, n);
		} else
			fprintf(stdout, "Wrong Algorithm type");
		fprintf(stdout, "\n");
	}
	if (!cfg.noout && !cfg.x509) {
		if (cfg.outformat == FORMAT_ASN1)
			i = i2d_X509_REQ_bio(out, req);
		else if (cfg.outformat == FORMAT_PEM) {
			if (cfg.newhdr)
				i = PEM_write_bio_X509_REQ_NEW(out, req);
			else
				i = PEM_write_bio_X509_REQ(out, req);
		} else {
			BIO_printf(bio_err, "bad output format specified for outfile\n");
			goto end;
		}
		if (!i) {
			BIO_printf(bio_err, "unable to write X509 request\n");
			goto end;
		}
	}
	if (!cfg.noout && cfg.x509 && (x509ss != NULL)) {
		if (cfg.outformat == FORMAT_ASN1)
			i = i2d_X509_bio(out, x509ss);
		else if (cfg.outformat == FORMAT_PEM)
			i = PEM_write_bio_X509(out, x509ss);
		else {
			BIO_printf(bio_err, "bad output format specified for outfile\n");
			goto end;
		}
		if (!i) {
			BIO_printf(bio_err, "unable to write X509 certificate\n");
			goto end;
		}
	}
	ex = 0;
 end:
	if (ex) {
		ERR_print_errors(bio_err);
	}
	if ((req_conf != NULL) && (req_conf != config))
		NCONF_free(req_conf);
	NCONF_free(addext_conf);
	BIO_free(cfg.addext_bio);
	BIO_free(in);
	BIO_free_all(out);
	EVP_PKEY_free(pkey);
	if (genctx)
		EVP_PKEY_CTX_free(genctx);
	if (cfg.pkeyopts)
		sk_OPENSSL_STRING_free(cfg.pkeyopts);
	if (cfg.sigopts)
		sk_OPENSSL_STRING_free(cfg.sigopts);
	lh_OPENSSL_STRING_doall(cfg.addexts, (LHASH_DOALL_FN_TYPE)exts_cleanup);
	lh_OPENSSL_STRING_free(cfg.addexts);
	free(keyalgstr);
	X509_REQ_free(req);
	X509_free(x509ss);
	ASN1_INTEGER_free(cfg.serial);
	if (cfg.passargin && passin)
		free(passin);
	if (cfg.passargout && passout)
		free(passout);
	OBJ_cleanup();

	return (ex);
}

static int
make_REQ(X509_REQ * req, EVP_PKEY * pkey, char *subj, int multirdn,
    int attribs, unsigned long chtype)
{
	int ret = 0, i;
	char no_prompt = 0;
	STACK_OF(CONF_VALUE) * dn_sk, *attr_sk = NULL;
	char *tmp, *dn_sect, *attr_sect;

	tmp = NCONF_get_string(req_conf, SECTION, PROMPT);
	if (tmp == NULL)
		ERR_clear_error();
	if ((tmp != NULL) && !strcmp(tmp, "no"))
		no_prompt = 1;

	dn_sect = NCONF_get_string(req_conf, SECTION, DISTINGUISHED_NAME);
	if (dn_sect == NULL) {
		BIO_printf(bio_err, "unable to find '%s' in config\n",
		    DISTINGUISHED_NAME);
		goto err;
	}
	dn_sk = NCONF_get_section(req_conf, dn_sect);
	if (dn_sk == NULL) {
		BIO_printf(bio_err, "unable to get '%s' section\n", dn_sect);
		goto err;
	}
	attr_sect = NCONF_get_string(req_conf, SECTION, ATTRIBUTES);
	if (attr_sect == NULL) {
		ERR_clear_error();
		attr_sk = NULL;
	} else {
		attr_sk = NCONF_get_section(req_conf, attr_sect);
		if (attr_sk == NULL) {
			BIO_printf(bio_err, "unable to get '%s' section\n", attr_sect);
			goto err;
		}
	}

	/* setup version number */
	if (!X509_REQ_set_version(req, 0L))
		goto err;	/* version 1 */

	if (no_prompt)
		i = auto_info(req, dn_sk, attr_sk, attribs, chtype);
	else {
		if (subj)
			i = build_subject(req, subj, chtype, multirdn);
		else
			i = prompt_info(req, dn_sk, dn_sect, attr_sk, attr_sect, attribs, chtype);
	}
	if (!i)
		goto err;

	if (!X509_REQ_set_pubkey(req, pkey))
		goto err;

	ret = 1;
 err:
	return (ret);
}

/*
 * subject is expected to be in the format /type0=value0/type1=value1/type2=...
 * where characters may be escaped by \
 */
static int
build_subject(X509_REQ * req, char *subject, unsigned long chtype, int multirdn)
{
	X509_NAME *n;

	if (!(n = parse_name(subject, chtype, multirdn)))
		return 0;

	if (!X509_REQ_set_subject_name(req, n)) {
		X509_NAME_free(n);
		return 0;
	}
	X509_NAME_free(n);
	return 1;
}


static int
prompt_info(X509_REQ * req,
    STACK_OF(CONF_VALUE) * dn_sk, char *dn_sect,
    STACK_OF(CONF_VALUE) * attr_sk, char *attr_sect, int attribs,
    unsigned long chtype)
{
	int i;
	char *p, *q;
	char buf[100];
	int nid, mval;
	long n_min, n_max;
	char *type, *value;
	const char *def;
	CONF_VALUE *v;
	X509_NAME *subj;
	subj = X509_REQ_get_subject_name(req);

	if (!cfg.batch) {
		BIO_printf(bio_err, "You are about to be asked to enter information that will be incorporated\n");
		BIO_printf(bio_err, "into your certificate request.\n");
		BIO_printf(bio_err, "What you are about to enter is what is called a Distinguished Name or a DN.\n");
		BIO_printf(bio_err, "There are quite a few fields but you can leave some blank\n");
		BIO_printf(bio_err, "For some fields there will be a default value,\n");
		BIO_printf(bio_err, "If you enter '.', the field will be left blank.\n");
		BIO_printf(bio_err, "-----\n");
	}
	if (sk_CONF_VALUE_num(dn_sk)) {
		i = -1;
 start:		for (;;) {
			int ret;
			i++;
			if (sk_CONF_VALUE_num(dn_sk) <= i)
				break;

			v = sk_CONF_VALUE_value(dn_sk, i);
			p = q = NULL;
			type = v->name;
			if (!check_end(type, "_min") || !check_end(type, "_max") ||
			    !check_end(type, "_default") ||
			    !check_end(type, "_value"))
				continue;
			/*
			 * Skip past any leading X. X: X, etc to allow for
			 * multiple instances
			 */
			for (p = v->name; *p; p++)
				if ((*p == ':') || (*p == ',') ||
				    (*p == '.')) {
					p++;
					if (*p)
						type = p;
					break;
				}
			if (*type == '+') {
				mval = -1;
				type++;
			} else
				mval = 0;
			/* If OBJ not recognised ignore it */
			if ((nid = OBJ_txt2nid(type)) == NID_undef)
				goto start;
			ret = snprintf(buf, sizeof buf, "%s_default", v->name);
			if (ret < 0 || ret >= sizeof(buf)) {
				BIO_printf(bio_err, "Name '%s' too long for default\n",
				    v->name);
				return 0;
			}
			if ((def = NCONF_get_string(req_conf, dn_sect, buf)) == NULL) {
				ERR_clear_error();
				def = "";
			}
			ret = snprintf(buf, sizeof buf, "%s_value", v->name);
			if (ret < 0 || ret >= sizeof(buf)) {
				BIO_printf(bio_err, "Name '%s' too long for value\n",
				    v->name);
				return 0;
			}
			if ((value = NCONF_get_string(req_conf, dn_sect, buf)) == NULL) {
				ERR_clear_error();
				value = NULL;
			}
			ret = snprintf(buf, sizeof buf, "%s_min", v->name);
			if (ret < 0 || ret >= sizeof(buf)) {
				BIO_printf(bio_err, "Name '%s' too long for min\n",
				    v->name);
				return 0;
			}
			if (!NCONF_get_number(req_conf, dn_sect, buf, &n_min)) {
				ERR_clear_error();
				n_min = -1;
			}
			ret = snprintf(buf, sizeof buf, "%s_max", v->name);
			if (ret < 0 || ret >= sizeof(buf)) {
				BIO_printf(bio_err, "Name '%s' too long for max\n",
				    v->name);
				return 0;
			}
			if (!NCONF_get_number(req_conf, dn_sect, buf, &n_max)) {
				ERR_clear_error();
				n_max = -1;
			}
			if (!add_DN_object(subj, v->value, def, value, nid,
				n_min, n_max, chtype, mval))
				return 0;
		}
		if (X509_NAME_entry_count(subj) == 0) {
			BIO_printf(bio_err, "error, no objects specified in config file\n");
			return 0;
		}
		if (attribs) {
			if ((attr_sk != NULL) && (sk_CONF_VALUE_num(attr_sk) > 0) &&
			    (!cfg.batch)) {
				BIO_printf(bio_err,
				    "\nPlease enter the following 'extra' attributes\n");
				BIO_printf(bio_err,
				    "to be sent with your certificate request\n");
			}
			i = -1;
start2:			for (;;) {
				int ret;
				i++;
				if ((attr_sk == NULL) ||
				    (sk_CONF_VALUE_num(attr_sk) <= i))
					break;

				v = sk_CONF_VALUE_value(attr_sk, i);
				type = v->name;
				if ((nid = OBJ_txt2nid(type)) == NID_undef)
					goto start2;
				ret = snprintf(buf, sizeof buf, "%s_default", type);
				if (ret < 0 || ret >= sizeof(buf)) {
					BIO_printf(bio_err, "Name '%s' too long for default\n",
					    v->name);
					return 0;
				}
				if ((def = NCONF_get_string(req_conf, attr_sect, buf))
				    == NULL) {
					ERR_clear_error();
					def = "";
				}
				ret = snprintf(buf, sizeof buf, "%s_value", type);
				if (ret < 0 || ret >= sizeof(buf)) {
					BIO_printf(bio_err, "Name '%s' too long for value\n",
					    v->name);
					return 0;
				}
				if ((value = NCONF_get_string(req_conf, attr_sect, buf))
				    == NULL) {
					ERR_clear_error();
					value = NULL;
				}
				ret = snprintf(buf, sizeof buf, "%s_min", type);
				if (ret < 0 || ret >= sizeof(buf)) {
					BIO_printf(bio_err, "Name '%s' too long for min\n",
					    v->name);
					return 0;
				}
				if (!NCONF_get_number(req_conf, attr_sect, buf, &n_min)) {
					ERR_clear_error();
					n_min = -1;
				}
				ret = snprintf(buf, sizeof buf, "%s_max", type);
				if (ret < 0 || ret >= sizeof(buf)) {
					BIO_printf(bio_err, "Name '%s' too long for max\n",
					    v->name);
					return 0;
				}
				if (!NCONF_get_number(req_conf, attr_sect, buf, &n_max)) {
					ERR_clear_error();
					n_max = -1;
				}
				if (!add_attribute_object(req,
					v->value, def, value, nid, n_min, n_max, chtype))
					return 0;
			}
		}
	} else {
		BIO_printf(bio_err, "No template, please set one up.\n");
		return 0;
	}

	return 1;

}

static int
auto_info(X509_REQ * req, STACK_OF(CONF_VALUE) * dn_sk,
    STACK_OF(CONF_VALUE) * attr_sk, int attribs, unsigned long chtype)
{
	int i;
	char *p, *q;
	char *type;
	CONF_VALUE *v;
	X509_NAME *subj;

	subj = X509_REQ_get_subject_name(req);

	for (i = 0; i < sk_CONF_VALUE_num(dn_sk); i++) {
		int mval;
		v = sk_CONF_VALUE_value(dn_sk, i);
		p = q = NULL;
		type = v->name;
		/*
		 * Skip past any leading X. X: X, etc to allow for multiple
		 * instances
		 */
		for (p = v->name; *p; p++)
			if ((*p == ':') || (*p == ',') || (*p == '.')) {
				p++;
				if (*p)
					type = p;
				break;
			}
		if (*p == '+') {
			p++;
			mval = -1;
		} else
			mval = 0;
		if (!X509_NAME_add_entry_by_txt(subj, type, chtype,
			(unsigned char *) v->value, -1, -1, mval))
			return 0;

	}

	if (!X509_NAME_entry_count(subj)) {
		BIO_printf(bio_err, "error, no objects specified in config file\n");
		return 0;
	}
	if (attribs) {
		for (i = 0; i < sk_CONF_VALUE_num(attr_sk); i++) {
			v = sk_CONF_VALUE_value(attr_sk, i);
			if (!X509_REQ_add1_attr_by_txt(req, v->name, chtype,
				(unsigned char *) v->value, -1))
				return 0;
		}
	}
	return 1;
}


static int
add_DN_object(X509_NAME * n, char *text, const char *def, char *value,
    int nid, int n_min, int n_max, unsigned long chtype, int mval)
{
	int i, ret = 0;
	char buf[1024];
 start:
	if (!cfg.batch)
		BIO_printf(bio_err, "%s [%s]:", text, def);
	(void) BIO_flush(bio_err);
	if (value != NULL) {
		strlcpy(buf, value, sizeof buf);
		strlcat(buf, "\n", sizeof buf);
		BIO_printf(bio_err, "%s\n", value);
	} else {
		buf[0] = '\0';
		if (!cfg.batch) {
			if (!fgets(buf, sizeof buf, stdin))
				return 0;
		} else {
			buf[0] = '\n';
			buf[1] = '\0';
		}
	}

	if (buf[0] == '\0')
		return (0);
	else if (buf[0] == '\n') {
		if ((def == NULL) || (def[0] == '\0'))
			return (1);
		strlcpy(buf, def, sizeof buf);
		strlcat(buf, "\n", sizeof buf);
	} else if ((buf[0] == '.') && (buf[1] == '\n'))
		return (1);

	i = strlen(buf);
	if (buf[i - 1] != '\n') {
		BIO_printf(bio_err, "weird input :-(\n");
		return (0);
	}
	buf[--i] = '\0';
	if (!req_check_len(i, n_min, n_max))
		goto start;
	if (!X509_NAME_add_entry_by_NID(n, nid, chtype,
		(unsigned char *) buf, -1, -1, mval))
		goto err;
	ret = 1;
 err:
	return (ret);
}

static int
add_attribute_object(X509_REQ * req, char *text, const char *def,
    char *value, int nid, int n_min,
    int n_max, unsigned long chtype)
{
	int i;
	static char buf[1024];

 start:
	if (!cfg.batch)
		BIO_printf(bio_err, "%s [%s]:", text, def);
	(void) BIO_flush(bio_err);
	if (value != NULL) {
		strlcpy(buf, value, sizeof buf);
		strlcat(buf, "\n", sizeof buf);
		BIO_printf(bio_err, "%s\n", value);
	} else {
		buf[0] = '\0';
		if (!cfg.batch) {
			if (!fgets(buf, sizeof buf, stdin))
				return 0;
		} else {
			buf[0] = '\n';
			buf[1] = '\0';
		}
	}

	if (buf[0] == '\0')
		return (0);
	else if (buf[0] == '\n') {
		if ((def == NULL) || (def[0] == '\0'))
			return (1);
		strlcpy(buf, def, sizeof buf);
		strlcat(buf, "\n", sizeof buf);
	} else if ((buf[0] == '.') && (buf[1] == '\n'))
		return (1);

	i = strlen(buf);
	if (buf[i - 1] != '\n') {
		BIO_printf(bio_err, "weird input :-(\n");
		return (0);
	}
	buf[--i] = '\0';
	if (!req_check_len(i, n_min, n_max))
		goto start;

	if (!X509_REQ_add1_attr_by_NID(req, nid, chtype,
		(unsigned char *) buf, -1)) {
		BIO_printf(bio_err, "Error adding attribute\n");
		ERR_print_errors(bio_err);
		goto err;
	}
	return (1);
 err:
	return (0);
}

static int
req_check_len(int len, int n_min, int n_max)
{
	if ((n_min > 0) && (len < n_min)) {
		BIO_printf(bio_err, "string is too short, it needs to be at least %d bytes long\n", n_min);
		return (0);
	}
	if ((n_max >= 0) && (len > n_max)) {
		BIO_printf(bio_err, "string is too long, it needs to be less than  %d bytes long\n", n_max);
		return (0);
	}
	return (1);
}

/* Check if the end of a string matches 'end' */
static int
check_end(const char *str, const char *end)
{
	int elen, slen;
	const char *tmp;
	elen = strlen(end);
	slen = strlen(str);
	if (elen > slen)
		return 1;
	tmp = str + slen - elen;
	return strcmp(tmp, end);
}

static EVP_PKEY_CTX *
set_keygen_ctx(BIO * err, const char *gstr, int *pkey_type,
    long *pkeylen, char **palgnam)
{
	EVP_PKEY_CTX *gctx = NULL;
	EVP_PKEY *param = NULL;
	long keylen = -1;
	BIO *pbio = NULL;
	const char *paramfile = NULL;
	const char *errstr;

	if (gstr == NULL) {
		*pkey_type = EVP_PKEY_RSA;
		keylen = *pkeylen;
	} else if (gstr[0] >= '0' && gstr[0] <= '9') {
		*pkey_type = EVP_PKEY_RSA;
		keylen = strtonum(gstr, 0, LONG_MAX, &errstr);
		if (errstr) {
			BIO_printf(err, "bad algorithm %s: %s\n", gstr, errstr);
			return NULL;
		}
		*pkeylen = keylen;
	} else if (!strncmp(gstr, "param:", 6))
		paramfile = gstr + 6;
	else {
		const char *p = strchr(gstr, ':');
		int len;
		const EVP_PKEY_ASN1_METHOD *ameth;

		if (p)
			len = p - gstr;
		else
			len = strlen(gstr);

		ameth = EVP_PKEY_asn1_find_str(NULL, gstr, len);

		if (!ameth) {
			BIO_printf(err, "Unknown algorithm %.*s\n", len, gstr);
			return NULL;
		}
		EVP_PKEY_asn1_get0_info(NULL, pkey_type, NULL, NULL, NULL,
		    ameth);
		if (*pkey_type == EVP_PKEY_RSA) {
			if (p) {
				keylen = strtonum(p + 1, 0, LONG_MAX, &errstr);
				if (errstr) {
					BIO_printf(err, "bad algorithm %s: %s\n",
					    p + 1, errstr);
					return NULL;
				}
				*pkeylen = keylen;
			} else
				keylen = *pkeylen;
		} else if (p)
			paramfile = p + 1;
	}

	if (paramfile) {
		pbio = BIO_new_file(paramfile, "r");
		if (!pbio) {
			BIO_printf(err, "Can't open parameter file %s\n",
			    paramfile);
			return NULL;
		}
		param = PEM_read_bio_Parameters(pbio, NULL);

		if (!param) {
			X509 *x;
			(void) BIO_reset(pbio);
			x = PEM_read_bio_X509(pbio, NULL, NULL, NULL);
			if (x) {
				param = X509_get_pubkey(x);
				X509_free(x);
			}
		}
		BIO_free(pbio);

		if (!param) {
			BIO_printf(err, "Error reading parameter file %s\n",
			    paramfile);
			return NULL;
		}
		if (*pkey_type == -1)
			*pkey_type = EVP_PKEY_id(param);
		else if (*pkey_type != EVP_PKEY_base_id(param)) {
			BIO_printf(err, "Key Type does not match parameters\n");
			EVP_PKEY_free(param);
			return NULL;
		}
	}
	if (palgnam) {
		const EVP_PKEY_ASN1_METHOD *ameth;
		const char *anam;
		ameth = EVP_PKEY_asn1_find(NULL, *pkey_type);
		if (!ameth) {
			BIO_puts(err, "Internal error: can't find key algorithm\n");
			return NULL;
		}
		EVP_PKEY_asn1_get0_info(NULL, NULL, NULL, NULL, &anam, ameth);
		*palgnam = strdup(anam);
	}
	if (param) {
		gctx = EVP_PKEY_CTX_new(param, NULL);
		*pkeylen = EVP_PKEY_bits(param);
		EVP_PKEY_free(param);
	} else
		gctx = EVP_PKEY_CTX_new_id(*pkey_type, NULL);

	if (!gctx) {
		BIO_puts(err, "Error allocating keygen context\n");
		ERR_print_errors(err);
		return NULL;
	}
	if (EVP_PKEY_keygen_init(gctx) <= 0) {
		BIO_puts(err, "Error initializing keygen context\n");
		ERR_print_errors(err);
		return NULL;
	}
	if ((*pkey_type == EVP_PKEY_RSA) && (keylen != -1)) {
		if (EVP_PKEY_CTX_set_rsa_keygen_bits(gctx, keylen) <= 0) {
			BIO_puts(err, "Error setting RSA keysize\n");
			ERR_print_errors(err);
			EVP_PKEY_CTX_free(gctx);
			return NULL;
		}
	}

	return gctx;
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

static int
do_sign_init(BIO * err, EVP_MD_CTX * ctx, EVP_PKEY * pkey,
    const EVP_MD * md, STACK_OF(OPENSSL_STRING) * sigopts)
{
	EVP_PKEY_CTX *pkctx = NULL;
	int default_nid;
	int i;

	if (EVP_PKEY_get_default_digest_nid(pkey, &default_nid) == 2 &&
	    default_nid == NID_undef) {
		/* The digest is required to be EVP_md_null() (EdDSA). */
		md = EVP_md_null();
	}

	if (!EVP_DigestSignInit(ctx, &pkctx, md, NULL, pkey))
		return 0;
	for (i = 0; i < sk_OPENSSL_STRING_num(sigopts); i++) {
		char *sigopt = sk_OPENSSL_STRING_value(sigopts, i);
		if (pkey_ctrl_string(pkctx, sigopt) <= 0) {
			BIO_printf(err, "parameter error \"%s\"\n", sigopt);
			ERR_print_errors(bio_err);
			return 0;
		}
	}
	return 1;
}

int
do_X509_sign(BIO * err, X509 * x, EVP_PKEY * pkey, const EVP_MD * md,
    STACK_OF(OPENSSL_STRING) * sigopts)
{
	EVP_MD_CTX *mctx;
	int rv;

	if ((mctx = EVP_MD_CTX_new()) == NULL)
		return 0;

	rv = do_sign_init(err, mctx, pkey, md, sigopts);
	if (rv > 0)
		rv = X509_sign_ctx(x, mctx);

	EVP_MD_CTX_free(mctx);

	return rv > 0;
}


int
do_X509_REQ_sign(BIO * err, X509_REQ * x, EVP_PKEY * pkey, const EVP_MD * md,
    STACK_OF(OPENSSL_STRING) * sigopts)
{
	EVP_MD_CTX *mctx;
	int rv;

	if ((mctx = EVP_MD_CTX_new()) == NULL)
		return 0;

	rv = do_sign_init(err, mctx, pkey, md, sigopts);
	if (rv > 0)
		rv = X509_REQ_sign_ctx(x, mctx);

	EVP_MD_CTX_free(mctx);

	return rv > 0;
}



int
do_X509_CRL_sign(BIO * err, X509_CRL * x, EVP_PKEY * pkey, const EVP_MD * md,
    STACK_OF(OPENSSL_STRING) * sigopts)
{
	int rv;
	EVP_MD_CTX *mctx;

	if ((mctx = EVP_MD_CTX_new()) == NULL)
		return 0;

	rv = do_sign_init(err, mctx, pkey, md, sigopts);
	if (rv > 0)
		rv = X509_CRL_sign_ctx(x, mctx);

	EVP_MD_CTX_free(mctx);

	return rv > 0;
}

static unsigned long
ext_name_hash(const OPENSSL_STRING *a)
{
	return lh_strhash((const char *)a);
}

static int
ext_name_cmp(const OPENSSL_STRING *a, const OPENSSL_STRING *b)
{
	return strcmp((const char *)a, (const char *)b);
}

static void
exts_cleanup(OPENSSL_STRING *x)
{
	free((char *)x);
}

/*
 * Is the |kv| key already duplicated ? This is remarkably tricky to get right.
 * Return 0 if unique, -1 on runtime error; 1 if found or a syntax error.
 */
static int
duplicated(LHASH_OF(OPENSSL_STRING) *addexts, char *kv)
{
	char *p;
	size_t off;

	/* Check syntax. */
	/* Skip leading whitespace, make a copy. */
	while (*kv && isspace(*kv))
		if (*++kv == '\0')
			return 1;
	if ((p = strchr(kv, '=')) == NULL)
		return 1;
	off = p - kv;
	if ((kv = strdup(kv)) == NULL)
		return -1;

	/* Skip trailing space before the equal sign. */
	for (p = kv + off; p > kv; --p)
		if (!isspace(p[-1]))
			break;
	if (p == kv) {
		free(kv);
		return 1;
	}
	*p = '\0';

	/* See if "key" is there by attempting to add it. */
	if ((p = (char *)lh_OPENSSL_STRING_insert(addexts, (OPENSSL_STRING*)kv))
	    != NULL || lh_OPENSSL_STRING_error(addexts)) {
		free(p != NULL ? p : kv);
		return -1;
	}

	return 0;
}
