/* $OpenBSD: ca.c,v 1.58 2024/02/04 13:08:29 tb Exp $ */
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

/* The PPKI stuff has been donated by Jeff Barber <jeffb@issl.atl.hp.com> */

#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "apps.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/ocsp.h>
#include <openssl/pem.h>
#include <openssl/txt_db.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#define BASE_SECTION		"ca"

#define ENV_DEFAULT_CA		"default_ca"

#define STRING_MASK		"string_mask"
#define UTF8_IN			"utf8"

#define ENV_NEW_CERTS_DIR	"new_certs_dir"
#define ENV_CERTIFICATE 	"certificate"
#define ENV_SERIAL		"serial"
#define ENV_CRLNUMBER		"crlnumber"
#define ENV_PRIVATE_KEY		"private_key"
#define ENV_DEFAULT_DAYS 	"default_days"
#define ENV_DEFAULT_STARTDATE 	"default_startdate"
#define ENV_DEFAULT_ENDDATE 	"default_enddate"
#define ENV_DEFAULT_CRL_DAYS 	"default_crl_days"
#define ENV_DEFAULT_CRL_HOURS 	"default_crl_hours"
#define ENV_DEFAULT_MD		"default_md"
#define ENV_DEFAULT_EMAIL_DN	"email_in_dn"
#define ENV_PRESERVE		"preserve"
#define ENV_POLICY      	"policy"
#define ENV_EXTENSIONS      	"x509_extensions"
#define ENV_CRLEXT      	"crl_extensions"
#define ENV_MSIE_HACK		"msie_hack"
#define ENV_NAMEOPT		"name_opt"
#define ENV_CERTOPT		"cert_opt"
#define ENV_EXTCOPY		"copy_extensions"
#define ENV_UNIQUE_SUBJECT	"unique_subject"

#define ENV_DATABASE		"database"

/* Additional revocation information types */

#define REV_NONE		0	/* No addditional information */
#define REV_CRL_REASON		1	/* Value is CRL reason code */
#define REV_HOLD		2	/* Value is hold instruction */
#define REV_KEY_COMPROMISE	3	/* Value is cert key compromise time */
#define REV_CA_COMPROMISE	4	/* Value is CA key compromise time */

static void lookup_fail(const char *name, const char *tag);
static int certify(X509 **xret, char *infile, EVP_PKEY *pkey, X509 *x509,
    const EVP_MD *dgst, STACK_OF(OPENSSL_STRING) *sigopts,
    STACK_OF(CONF_VALUE) *policy, CA_DB *db, BIGNUM *serial, char *subj,
    unsigned long chtype, int multirdn, int email_dn, char *startdate,
    char *enddate, long days, int batch, char *ext_sect, CONF *conf,
    int verbose, unsigned long certopt, unsigned long nameopt,
    int default_op, int ext_copy, int selfsign);
static int certify_cert(X509 **xret, char *infile, EVP_PKEY *pkey,
    X509 *x509, const EVP_MD *dgst, STACK_OF(OPENSSL_STRING) *sigopts,
    STACK_OF(CONF_VALUE) *policy, CA_DB *db, BIGNUM *serial, char *subj,
    unsigned long chtype, int multirdn, int email_dn, char *startdate,
    char *enddate, long days, int batch, char *ext_sect, CONF *conf,
    int verbose, unsigned long certopt, unsigned long nameopt, int default_op,
    int ext_copy);
static int certify_spkac(X509 **xret, char *infile, EVP_PKEY *pkey,
    X509 *x509, const EVP_MD *dgst, STACK_OF(OPENSSL_STRING) *sigopts,
    STACK_OF(CONF_VALUE) *policy, CA_DB *db, BIGNUM *serial, char *subj,
    unsigned long chtype, int multirdn, int email_dn, char *startdate,
    char *enddate, long days, char *ext_sect, CONF *conf, int verbose,
    unsigned long certopt, unsigned long nameopt, int default_op, int ext_copy);
static int write_new_certificate(BIO *bp, X509 *x, int output_der,
    int notext);
static int do_body(X509 **xret, EVP_PKEY *pkey, X509 *x509,
    const EVP_MD *dgst, STACK_OF(OPENSSL_STRING) *sigopts,
    STACK_OF(CONF_VALUE) *policy, CA_DB *db, BIGNUM *serial, char *subj,
    unsigned long chtype, int multirdn, int email_dn, char *startdate,
    char *enddate, long days, int batch, int verbose, X509_REQ *req,
    char *ext_sect, CONF *conf, unsigned long certopt, unsigned long nameopt,
    int default_op, int ext_copy, int selfsign);
static int do_revoke(X509 *x509, CA_DB *db, int ext, char *extval);
static int get_certificate_status(const char *serial, CA_DB *db);
static int do_updatedb(CA_DB *db);
static int check_time_format(const char *str);
static char *bin2hex(unsigned char *, size_t);
char *make_revocation_str(int rev_type, char *rev_arg);
int make_revoked(X509_REVOKED *rev, const char *str);
int old_entry_print(BIO *bp, ASN1_OBJECT *obj, ASN1_STRING *str);

static CONF *conf = NULL;
static CONF *extconf = NULL;

static struct {
	int batch;
	char *certfile;
	unsigned long chtype;
	char *configfile;
	int create_serial;
	char *crl_ext;
	long crldays;
	long crlhours;
	long crlsec;
	long days;
	int dorevoke;
	int doupdatedb;
	int email_dn;
	char *enddate;
	char *extensions;
	char *extfile;
	int gencrl;
	char *infile;
	char **infiles;
	int infiles_num;
	char *key;
	char *keyfile;
	int keyform;
	char *md;
	int multirdn;
	int msie_hack;
	int notext;
	char *outdir;
	char *outfile;
	char *passargin;
	char *policy;
	int preserve;
	int req;
	char *rev_arg;
	int rev_type;
	char *serial_status;
	char *section;
	int selfsign;
	STACK_OF(OPENSSL_STRING) *sigopts;
	char *spkac_file;
	char *ss_cert_file;
	char *startdate;
	char *subj;
	int verbose;
} cfg;

static int
ca_opt_chtype_utf8(void)
{
	cfg.chtype = MBSTRING_UTF8;
	return (0);
}

static int
ca_opt_crl_ca_compromise(char *arg)
{
	cfg.rev_arg = arg;
	cfg.rev_type = REV_CA_COMPROMISE;
	return (0);
}

static int
ca_opt_crl_compromise(char *arg)
{
	cfg.rev_arg = arg;
	cfg.rev_type = REV_KEY_COMPROMISE;
	return (0);
}

static int
ca_opt_crl_hold(char *arg)
{
	cfg.rev_arg = arg;
	cfg.rev_type = REV_HOLD;
	return (0);
}

static int
ca_opt_crl_reason(char *arg)
{
	cfg.rev_arg = arg;
	cfg.rev_type = REV_CRL_REASON;
	return (0);
}

static int
ca_opt_in(char *arg)
{
	cfg.infile = arg;
	cfg.req = 1;
	return (0);
}

static int
ca_opt_infiles(int argc, char **argv, int *argsused)
{
	cfg.infiles_num = argc - 1;
	if (cfg.infiles_num < 1)
		return (1);
	cfg.infiles = argv + 1;
	cfg.req = 1;
	*argsused = argc;
	return (0);
}

static int
ca_opt_revoke(char *arg)
{
	cfg.infile = arg;
	cfg.dorevoke = 1;
	return (0);
}

static int
ca_opt_sigopt(char *arg)
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
ca_opt_spkac(char *arg)
{
	cfg.spkac_file = arg;
	cfg.req = 1;
	return (0);
}

static int
ca_opt_ss_cert(char *arg)
{
	cfg.ss_cert_file = arg;
	cfg.req = 1;
	return (0);
}

static const struct option ca_options[] = {
	{
		.name = "batch",
		.desc = "Operate in batch mode",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.batch,
	},
	{
		.name = "cert",
		.argname = "file",
		.desc = "File containing the CA certificate",
		.type = OPTION_ARG,
		.opt.arg = &cfg.certfile,
	},
	{
		.name = "config",
		.argname = "file",
		.desc = "Specify an alternative configuration file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.configfile,
	},
	{
		.name = "create_serial",
		.desc = "If reading serial fails, create a new random serial",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.create_serial,
	},
	{
		.name = "crl_CA_compromise",
		.argname = "time",
		.desc = "Set the compromise time and the revocation reason to\n"
		    "CACompromise",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ca_opt_crl_ca_compromise,
	},
	{
		.name = "crl_compromise",
		.argname = "time",
		.desc = "Set the compromise time and the revocation reason to\n"
		    "keyCompromise",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ca_opt_crl_compromise,
	},
	{
		.name = "crl_hold",
		.argname = "instruction",
		.desc = "Set the hold instruction and the revocation reason to\n"
		    "certificateHold",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ca_opt_crl_hold,
	},
	{
		.name = "crl_reason",
		.argname = "reason",
		.desc = "Revocation reason",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ca_opt_crl_reason,
	},
	{
		.name = "crldays",
		.argname = "days",
		.desc = "Number of days before the next CRL is due",
		.type = OPTION_ARG_LONG,
		.opt.lvalue = &cfg.crldays,
	},
	{
		.name = "crlexts",
		.argname = "section",
		.desc = "CRL extension section (override value in config file)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.crl_ext,
	},
	{
		.name = "crlhours",
		.argname = "hours",
		.desc = "Number of hours before the next CRL is due",
		.type = OPTION_ARG_LONG,
		.opt.lvalue = &cfg.crlhours,
	},
	{
		.name = "crlsec",
		.argname = "seconds",
		.desc = "Number of seconds before the next CRL is due",
		.type = OPTION_ARG_LONG,
		.opt.lvalue = &cfg.crlsec,
	},
	{
		.name = "days",
		.argname = "arg",
		.desc = "Number of days to certify the certificate for",
		.type = OPTION_ARG_LONG,
		.opt.lvalue = &cfg.days,
	},
	{
		.name = "enddate",
		.argname = "YYMMDDHHMMSSZ",
		.desc = "Certificate validity notAfter (overrides -days)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.enddate,
	},
	{
		.name = "extensions",
		.argname = "section",
		.desc = "Extension section (override value in config file)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.extensions,
	},
	{
		.name = "extfile",
		.argname = "file",
		.desc = "Configuration file with X509v3 extentions to add",
		.type = OPTION_ARG,
		.opt.arg = &cfg.extfile,
	},
	{
		.name = "gencrl",
		.desc = "Generate a new CRL",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.gencrl,
	},
	{
		.name = "in",
		.argname = "file",
		.desc = "Input file containing a single certificate request",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ca_opt_in,
	},
	{
		.name = "infiles",
		.argname = "...",
		.desc = "The last argument, certificate requests to process",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = ca_opt_infiles,
	},
	{
		.name = "key",
		.argname = "password",
		.desc = "Key to decode the private key if it is encrypted",
		.type = OPTION_ARG,
		.opt.arg = &cfg.key,
	},
	{
		.name = "keyfile",
		.argname = "file",
		.desc = "Private key file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.keyfile,
	},
	{
		.name = "keyform",
		.argname = "fmt",
		.desc = "Private key file format (DER or PEM (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.keyform,
	},
	{
		.name = "md",
		.argname = "alg",
		.desc = "Message digest to use",
		.type = OPTION_ARG,
		.opt.arg = &cfg.md,
	},
	{
		.name = "msie_hack",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.msie_hack,
	},
	{
		.name = "multivalue-rdn",
		.desc = "Enable support for multivalued RDNs",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.multirdn,
	},
	{
		.name = "name",
		.argname = "section",
		.desc = "Specifies the configuration file section to use",
		.type = OPTION_ARG,
		.opt.arg = &cfg.section,
	},
	{
		.name = "noemailDN",
		.desc = "Do not add the EMAIL field to the DN",
		.type = OPTION_VALUE,
		.opt.value = &cfg.email_dn,
		.value = 0,
	},
	{
		.name = "notext",
		.desc = "Do not print the generated certificate",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.notext,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output file (default stdout)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.outfile,
	},
	{
		.name = "outdir",
		.argname = "directory",
		.desc = " Directory to output certificates to",
		.type = OPTION_ARG,
		.opt.arg = &cfg.outdir,
	},
	{
		.name = "passin",
		.argname = "src",
		.desc = "Private key input password source",
		.type = OPTION_ARG,
		.opt.arg = &cfg.passargin,
	},
	{
		.name = "policy",
		.argname = "name",
		.desc = "The CA 'policy' to support",
		.type = OPTION_ARG,
		.opt.arg = &cfg.policy,
	},
	{
		.name = "preserveDN",
		.desc = "Do not re-order the DN",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.preserve,
	},
	{
		.name = "revoke",
		.argname = "file",
		.desc = "Revoke a certificate (given in file)",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ca_opt_revoke,
	},
	{
		.name = "selfsign",
		.desc = "Sign a certificate using the key associated with it",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.selfsign,
	},
	{
		.name = "sigopt",
		.argname = "nm:v",
		.desc = "Signature parameter in nm:v form",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ca_opt_sigopt,
	},
	{
		.name = "spkac",
		.argname = "file",
		.desc = "File contains DN and signed public key and challenge",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ca_opt_spkac,
	},
	{
		.name = "ss_cert",
		.argname = "file",
		.desc = "File contains a self signed certificate to sign",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ca_opt_ss_cert,
	},
	{
		.name = "startdate",
		.argname = "YYMMDDHHMMSSZ",
		.desc = "Certificate validity notBefore",
		.type = OPTION_ARG,
		.opt.arg = &cfg.startdate,
	},
	{
		.name = "status",
		.argname = "serial",
		.desc = "Shows certificate status given the serial number",
		.type = OPTION_ARG,
		.opt.arg = &cfg.serial_status,
	},
	{
		.name = "subj",
		.argname = "arg",
		.desc = "Use arg instead of request's subject",
		.type = OPTION_ARG,
		.opt.arg = &cfg.subj,
	},
	{
		.name = "updatedb",
		.desc = "Updates db for expired certificates",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.doupdatedb,
	},
	{
		.name = "utf8",
		.desc = "Input characters are in UTF-8 (default ASCII)",
		.type = OPTION_FUNC,
		.opt.func = ca_opt_chtype_utf8,
	},
	{
		.name = "verbose",
		.desc = "Verbose output during processing",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.verbose,
	},
	{ NULL },
};

static void
ca_usage(void)
{
	fprintf(stderr,
	    "usage: ca  [-batch] [-cert file] [-config file] [-create_serial]\n"
	    "    [-crl_CA_compromise time] [-crl_compromise time]\n"
	    "    [-crl_hold instruction] [-crl_reason reason] [-crldays days]\n"
	    "    [-crlexts section] [-crlhours hours] [-crlsec seconds]\n"
	    "    [-days arg] [-enddate date] [-extensions section]\n"
	    "    [-extfile file] [-gencrl] [-in file] [-infiles]\n"
	    "    [-key password] [-keyfile file] [-keyform pem | der]\n"
	    "    [-md alg] [-multivalue-rdn] [-name section]\n"
	    "    [-noemailDN] [-notext] [-out file] [-outdir directory]\n"
	    "    [-passin arg] [-policy name] [-preserveDN] [-revoke file]\n"
	    "    [-selfsign] [-sigopt nm:v] [-spkac file] [-ss_cert file]\n"
	    "    [-startdate date] [-status serial] [-subj arg] [-updatedb]\n"
	    "    [-utf8] [-verbose]\n\n");
	options_usage(ca_options);
	fprintf(stderr, "\n");
}

int
ca_main(int argc, char **argv)
{
	int free_key = 0;
	int total = 0;
	int total_done = 0;
	long errorline = -1;
	EVP_PKEY *pkey = NULL;
	int output_der = 0;
	char *serialfile = NULL;
	char *crlnumberfile = NULL;
	char *tmp_email_dn = NULL;
	BIGNUM *serial = NULL;
	BIGNUM *crlnumber = NULL;
	unsigned long nameopt = 0, certopt = 0;
	int default_op = 1;
	int ext_copy = EXT_COPY_NONE;
	X509 *x509 = NULL, *x509p = NULL;
	X509 *x = NULL;
	BIO *in = NULL, *out = NULL, *Sout = NULL, *Cout = NULL;
	char *dbfile = NULL;
	CA_DB *db = NULL;
	X509_CRL *crl = NULL;
	X509_REVOKED *r = NULL;
	ASN1_TIME *tmptm = NULL;
	ASN1_INTEGER *tmpserial;
	char *f;
	const char *p;
	char *const *pp;
	int i, j;
	const EVP_MD *dgst = NULL;
	STACK_OF(CONF_VALUE) *attribs = NULL;
	STACK_OF(X509) *cert_sk = NULL;
	char *tofree = NULL;
	DB_ATTR db_attr;
	int default_nid, rv;
	int ret = 1;

	if (pledge("stdio cpath wpath rpath tty", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.email_dn = 1;
	cfg.keyform = FORMAT_PEM;
	cfg.chtype = MBSTRING_ASC;
	cfg.rev_type = REV_NONE;

	conf = NULL;

	if (options_parse(argc, argv, ca_options, NULL, NULL) != 0) {
		ca_usage();
		goto err;
	}

	/*****************************************************************/
	tofree = NULL;
	if (cfg.configfile == NULL)
		cfg.configfile = getenv("OPENSSL_CONF");
	if (cfg.configfile == NULL) {
		if ((tofree = make_config_name()) == NULL) {
			BIO_printf(bio_err, "error making config file name\n");
			goto err;
		}
		cfg.configfile = tofree;
	}
	BIO_printf(bio_err, "Using configuration from %s\n",
	    cfg.configfile);
	conf = NCONF_new(NULL);
	if (NCONF_load(conf, cfg.configfile, &errorline) <= 0) {
		if (errorline <= 0)
			BIO_printf(bio_err,
			    "error loading the config file '%s'\n",
			    cfg.configfile);
		else
			BIO_printf(bio_err,
			    "error on line %ld of config file '%s'\n",
			    errorline, cfg.configfile);
		goto err;
	}
	free(tofree);
	tofree = NULL;

	/* Lets get the config section we are using */
	if (cfg.section == NULL) {
		cfg.section = NCONF_get_string(conf, BASE_SECTION,
		    ENV_DEFAULT_CA);
		if (cfg.section == NULL) {
			lookup_fail(BASE_SECTION, ENV_DEFAULT_CA);
			goto err;
		}
	}
	if (conf != NULL) {
		p = NCONF_get_string(conf, NULL, "oid_file");
		if (p == NULL)
			ERR_clear_error();
		if (p != NULL) {
			BIO *oid_bio;

			oid_bio = BIO_new_file(p, "r");
			if (oid_bio == NULL) {
				/*
				BIO_printf(bio_err,
				    "problems opening %s for extra oid's\n", p);
				ERR_print_errors(bio_err);
				*/
				ERR_clear_error();
			} else {
				OBJ_create_objects(oid_bio);
				BIO_free(oid_bio);
			}
		}
		if (!add_oid_section(bio_err, conf)) {
			ERR_print_errors(bio_err);
			goto err;
		}
	}
	f = NCONF_get_string(conf, cfg.section, STRING_MASK);
	if (f == NULL)
		ERR_clear_error();

	if (f != NULL && !ASN1_STRING_set_default_mask_asc(f)) {
		BIO_printf(bio_err,
		    "Invalid global string mask setting %s\n", f);
		goto err;
	}
	if (cfg.chtype != MBSTRING_UTF8) {
		f = NCONF_get_string(conf, cfg.section, UTF8_IN);
		if (f == NULL)
			ERR_clear_error();
		else if (strcmp(f, "yes") == 0)
			cfg.chtype = MBSTRING_UTF8;
	}
	db_attr.unique_subject = 1;
	p = NCONF_get_string(conf, cfg.section, ENV_UNIQUE_SUBJECT);
	if (p != NULL) {
		db_attr.unique_subject = parse_yesno(p, 1);
	} else
		ERR_clear_error();

	in = BIO_new(BIO_s_file());
	out = BIO_new(BIO_s_file());
	Sout = BIO_new(BIO_s_file());
	Cout = BIO_new(BIO_s_file());
	if ((in == NULL) || (out == NULL) || (Sout == NULL) || (Cout == NULL)) {
		ERR_print_errors(bio_err);
		goto err;
	}
	/*****************************************************************/
	/* report status of cert with serial number given on command line */
	if (cfg.serial_status) {
		if ((dbfile = NCONF_get_string(conf, cfg.section,
		    ENV_DATABASE)) == NULL) {
			lookup_fail(cfg.section, ENV_DATABASE);
			goto err;
		}
		db = load_index(dbfile, &db_attr);
		if (db == NULL)
			goto err;

		if (!index_index(db))
			goto err;

		if (get_certificate_status(cfg.serial_status, db) != 1)
			BIO_printf(bio_err, "Error verifying serial %s!\n",
			    cfg.serial_status);
		goto err;
	}
	/*****************************************************************/
	/* we definitely need a private key, so let's get it */

	if ((cfg.keyfile == NULL) &&
	    ((cfg.keyfile = NCONF_get_string(conf, cfg.section,
	    ENV_PRIVATE_KEY)) == NULL)) {
		lookup_fail(cfg.section, ENV_PRIVATE_KEY);
		goto err;
	}
	if (cfg.key == NULL) {
		free_key = 1;
		if (!app_passwd(bio_err, cfg.passargin, NULL,
		    &cfg.key, NULL)) {
			BIO_printf(bio_err, "Error getting password\n");
			goto err;
		}
	}
	pkey = load_key(bio_err, cfg.keyfile, cfg.keyform, 0,
	    cfg.key, "CA private key");
	if (cfg.key != NULL)
		explicit_bzero(cfg.key, strlen(cfg.key));
	if (pkey == NULL) {
		/* load_key() has already printed an appropriate message */
		goto err;
	}
	/*****************************************************************/
	/* we need a certificate */
	if (!cfg.selfsign || cfg.spkac_file != NULL ||
	    cfg.ss_cert_file != NULL || cfg.gencrl) {
		if ((cfg.certfile == NULL) &&
		    ((cfg.certfile = NCONF_get_string(conf,
		    cfg.section, ENV_CERTIFICATE)) == NULL)) {
			lookup_fail(cfg.section, ENV_CERTIFICATE);
			goto err;
		}
		x509 = load_cert(bio_err, cfg.certfile, FORMAT_PEM, NULL,
		    "CA certificate");
		if (x509 == NULL)
			goto err;

		if (!X509_check_private_key(x509, pkey)) {
			BIO_printf(bio_err,
			    "CA certificate and CA private key do not match\n");
			goto err;
		}
	}
	if (!cfg.selfsign)
		x509p = x509;

	f = NCONF_get_string(conf, BASE_SECTION, ENV_PRESERVE);
	if (f == NULL)
		ERR_clear_error();
	if ((f != NULL) && ((*f == 'y') || (*f == 'Y')))
		cfg.preserve = 1;
	f = NCONF_get_string(conf, BASE_SECTION, ENV_MSIE_HACK);
	if (f == NULL)
		ERR_clear_error();
	if ((f != NULL) && ((*f == 'y') || (*f == 'Y')))
		cfg.msie_hack = 1;

	f = NCONF_get_string(conf, cfg.section, ENV_NAMEOPT);

	if (f != NULL) {
		if (!set_name_ex(&nameopt, f)) {
			BIO_printf(bio_err,
			    "Invalid name options: \"%s\"\n", f);
			goto err;
		}
		default_op = 0;
	} else
		ERR_clear_error();

	f = NCONF_get_string(conf, cfg.section, ENV_CERTOPT);

	if (f != NULL) {
		if (!set_cert_ex(&certopt, f)) {
			BIO_printf(bio_err,
			    "Invalid certificate options: \"%s\"\n", f);
			goto err;
		}
		default_op = 0;
	} else
		ERR_clear_error();

	f = NCONF_get_string(conf, cfg.section, ENV_EXTCOPY);

	if (f != NULL) {
		if (!set_ext_copy(&ext_copy, f)) {
			BIO_printf(bio_err,
			    "Invalid extension copy option: \"%s\"\n", f);
			goto err;
		}
	} else
		ERR_clear_error();

	/*****************************************************************/
	/* lookup where to write new certificates */
	if (cfg.outdir == NULL && cfg.req) {
		if ((cfg.outdir = NCONF_get_string(conf,
		    cfg.section, ENV_NEW_CERTS_DIR)) == NULL) {
			BIO_printf(bio_err, "output directory %s not defined\n",
			    ENV_NEW_CERTS_DIR);
			goto err;
		}
	}
	/*****************************************************************/
	/* we need to load the database file */
	if ((dbfile = NCONF_get_string(conf, cfg.section,
	    ENV_DATABASE)) == NULL) {
		lookup_fail(cfg.section, ENV_DATABASE);
		goto err;
	}
	db = load_index(dbfile, &db_attr);
	if (db == NULL)
		goto err;

	/* Lets check some fields */
	for (i = 0; i < sk_OPENSSL_PSTRING_num(db->db->data); i++) {
		pp = sk_OPENSSL_PSTRING_value(db->db->data, i);
		if ((pp[DB_type][0] != DB_TYPE_REV) &&
		    (pp[DB_rev_date][0] != '\0')) {
			BIO_printf(bio_err,
			    "entry %d: not revoked yet, but has a revocation date\n",
			    i + 1);
			goto err;
		}
		if ((pp[DB_type][0] == DB_TYPE_REV) &&
		    !make_revoked(NULL, pp[DB_rev_date])) {
			BIO_printf(bio_err, " in entry %d\n", i + 1);
			goto err;
		}
		if (!check_time_format((char *) pp[DB_exp_date])) {
			BIO_printf(bio_err, "entry %d: invalid expiry date\n",
			    i + 1);
			goto err;
		}
		p = pp[DB_serial];
		j = strlen(p);
		if (*p == '-') {
			p++;
			j--;
		}
		if ((j & 1) || (j < 2)) {
			BIO_printf(bio_err,
			    "entry %d: bad serial number length (%d)\n",
			    i + 1, j);
			goto err;
		}
		while (*p) {
			if (!(((*p >= '0') && (*p <= '9')) ||
			    ((*p >= 'A') && (*p <= 'F')) ||
			    ((*p >= 'a') && (*p <= 'f')))) {
				BIO_printf(bio_err,
				    "entry %d: bad serial number characters, char pos %ld, char is '%c'\n",
				    i + 1, (long) (p - pp[DB_serial]), *p);
				goto err;
			}
			p++;
		}
	}
	if (cfg.verbose) {
		BIO_set_fp(out, stdout, BIO_NOCLOSE | BIO_FP_TEXT);
		TXT_DB_write(out, db->db);
		BIO_printf(bio_err, "%d entries loaded from the database\n",
		    sk_OPENSSL_PSTRING_num(db->db->data));
		BIO_printf(bio_err, "generating index\n");
	}
	if (!index_index(db))
		goto err;

	/*****************************************************************/
	/* Update the db file for expired certificates */
	if (cfg.doupdatedb) {
		if (cfg.verbose)
			BIO_printf(bio_err, "Updating %s ...\n", dbfile);

		i = do_updatedb(db);
		if (i == -1) {
			BIO_printf(bio_err, "Malloc failure\n");
			goto err;
		} else if (i == 0) {
			if (cfg.verbose)
				BIO_printf(bio_err,
				    "No entries found to mark expired\n");
		} else {
			if (!save_index(dbfile, "new", db))
				goto err;

			if (!rotate_index(dbfile, "new", "old"))
				goto err;

			if (cfg.verbose)
				BIO_printf(bio_err,
				    "Done. %d entries marked as expired\n", i);
		}
	}
	/*****************************************************************/
	/* Read extentions config file                                   */
	if (cfg.extfile != NULL) {
		extconf = NCONF_new(NULL);
		if (NCONF_load(extconf, cfg.extfile, &errorline) <= 0) {
			if (errorline <= 0)
				BIO_printf(bio_err,
				    "ERROR: loading the config file '%s'\n",
				    cfg.extfile);
			else
				BIO_printf(bio_err,
				    "ERROR: on line %ld of config file '%s'\n",
				    errorline, cfg.extfile);
			ret = 1;
			goto err;
		}
		if (cfg.verbose)
			BIO_printf(bio_err,
			    "Successfully loaded extensions file %s\n",
			    cfg.extfile);

		/* We can have sections in the ext file */
		if (cfg.extensions == NULL &&
		    (cfg.extensions = NCONF_get_string(extconf, "default",
		    "extensions")) == NULL)
			cfg.extensions = "default";
	}
	/*****************************************************************/
	if (cfg.req || cfg.gencrl) {
		if (cfg.outfile != NULL) {
			if (BIO_write_filename(Sout, cfg.outfile) <= 0) {
				perror(cfg.outfile);
				goto err;
			}
		} else {
			BIO_set_fp(Sout, stdout, BIO_NOCLOSE | BIO_FP_TEXT);
		}
	}

	rv = EVP_PKEY_get_default_digest_nid(pkey, &default_nid);
	if (rv == 2 && default_nid == NID_undef) {
		/* The digest is required to be EVP_md_null() (EdDSA). */
		dgst = EVP_md_null();
	} else {
		/* Ignore rv unless we need a valid default_nid. */
		if (cfg.md == NULL)
			cfg.md = NCONF_get_string(conf, cfg.section,
			    ENV_DEFAULT_MD);
		if (cfg.md == NULL) {
			lookup_fail(cfg.section, ENV_DEFAULT_MD);
			goto err;
		}
		if (strcmp(cfg.md, "default") == 0) {
			if (rv <= 0) {
				BIO_puts(bio_err, "no default digest\n");
				goto err;
			}
			cfg.md = (char *)OBJ_nid2sn(default_nid);
		}
		if (cfg.md == NULL)
			goto err;
		if ((dgst = EVP_get_digestbyname(cfg.md)) == NULL) {
			BIO_printf(bio_err, "%s is an unsupported "
			    "message digest type\n", cfg.md);
			goto err;
		}
	}
	if (cfg.req) {
		if ((cfg.email_dn == 1) &&
		    ((tmp_email_dn = NCONF_get_string(conf, cfg.section,
		    ENV_DEFAULT_EMAIL_DN)) != NULL)) {
			if (strcmp(tmp_email_dn, "no") == 0)
				cfg.email_dn = 0;
		}
		if (cfg.verbose)
			BIO_printf(bio_err, "message digest is %s\n",
			    OBJ_nid2ln(EVP_MD_type(dgst)));
		if ((cfg.policy == NULL) &&
		    ((cfg.policy = NCONF_get_string(conf,
		    cfg.section, ENV_POLICY)) == NULL)) {
			lookup_fail(cfg.section, ENV_POLICY);
			goto err;
		}
		if (cfg.verbose)
			BIO_printf(bio_err, "policy is %s\n", cfg.policy);

		if ((serialfile = NCONF_get_string(conf, cfg.section,
		    ENV_SERIAL)) == NULL) {
			lookup_fail(cfg.section, ENV_SERIAL);
			goto err;
		}
		if (extconf == NULL) {
			/*
			 * no '-extfile' option, so we look for extensions in
			 * the main configuration file
			 */
			if (cfg.extensions == NULL) {
				cfg.extensions = NCONF_get_string(conf,
				    cfg.section, ENV_EXTENSIONS);
				if (cfg.extensions == NULL)
					ERR_clear_error();
			}
			if (cfg.extensions != NULL) {
				/* Check syntax of file */
				X509V3_CTX ctx;
				X509V3_set_ctx_test(&ctx);
				X509V3_set_nconf(&ctx, conf);
				if (!X509V3_EXT_add_nconf(conf, &ctx,
				    cfg.extensions, NULL)) {
					BIO_printf(bio_err,
					    "Error Loading extension section %s\n",
					    cfg.extensions);
					ret = 1;
					goto err;
				}
			}
		}
		if (cfg.startdate == NULL) {
			cfg.startdate = NCONF_get_string(conf,
			    cfg.section, ENV_DEFAULT_STARTDATE);
			if (cfg.startdate == NULL)
				ERR_clear_error();
		}
		if (cfg.startdate == NULL)
			cfg.startdate = "today";

		if (cfg.enddate == NULL) {
			cfg.enddate = NCONF_get_string(conf,
			    cfg.section, ENV_DEFAULT_ENDDATE);
			if (cfg.enddate == NULL)
				ERR_clear_error();
		}
		if (cfg.days == 0 && cfg.enddate == NULL) {
			if (!NCONF_get_number(conf, cfg.section,
				ENV_DEFAULT_DAYS, &cfg.days))
				cfg.days = 0;
		}
		if (cfg.enddate == NULL && cfg.days == 0) {
			BIO_printf(bio_err,
			    "cannot lookup how many days to certify for\n");
			goto err;
		}
		if ((serial = load_serial(serialfile, cfg.create_serial,
		    NULL)) == NULL) {
			BIO_printf(bio_err,
			    "error while loading serial number\n");
			goto err;
		}
		if (cfg.verbose) {
			if (BN_is_zero(serial))
				BIO_printf(bio_err,
				    "next serial number is 00\n");
			else {
				if ((f = BN_bn2hex(serial)) == NULL)
					goto err;
				BIO_printf(bio_err,
				    "next serial number is %s\n", f);
				free(f);
			}
		}
		if ((attribs = NCONF_get_section(conf, cfg.policy)) ==
		    NULL) {
			BIO_printf(bio_err, "unable to find 'section' for %s\n",
			    cfg.policy);
			goto err;
		}
		if ((cert_sk = sk_X509_new_null()) == NULL) {
			BIO_printf(bio_err, "Memory allocation failure\n");
			goto err;
		}
		if (cfg.spkac_file != NULL) {
			total++;
			j = certify_spkac(&x, cfg.spkac_file, pkey, x509,
			    dgst, cfg.sigopts, attribs, db, serial,
			    cfg.subj, cfg.chtype,
			    cfg.multirdn, cfg.email_dn,
			    cfg.startdate, cfg.enddate,
			    cfg.days, cfg.extensions, conf,
			    cfg.verbose, certopt, nameopt, default_op,
			    ext_copy);
			if (j < 0)
				goto err;
			if (j > 0) {
				total_done++;
				BIO_printf(bio_err, "\n");
				if (!BN_add_word(serial, 1))
					goto err;
				if (!sk_X509_push(cert_sk, x)) {
					BIO_printf(bio_err,
					    "Memory allocation failure\n");
					goto err;
				}
				if (cfg.outfile != NULL) {
					output_der = 1;
					cfg.batch = 1;
				}
			}
		}
		if (cfg.ss_cert_file != NULL) {
			total++;
			j = certify_cert(&x, cfg.ss_cert_file, pkey, x509,
			    dgst, cfg.sigopts, attribs, db, serial,
			    cfg.subj, cfg.chtype,
			    cfg.multirdn, cfg.email_dn,
			    cfg.startdate, cfg.enddate,
			    cfg.days, cfg.batch,
			    cfg.extensions, conf, cfg.verbose,
			    certopt, nameopt, default_op, ext_copy);
			if (j < 0)
				goto err;
			if (j > 0) {
				total_done++;
				BIO_printf(bio_err, "\n");
				if (!BN_add_word(serial, 1))
					goto err;
				if (!sk_X509_push(cert_sk, x)) {
					BIO_printf(bio_err,
					    "Memory allocation failure\n");
					goto err;
				}
			}
		}
		if (cfg.infile != NULL) {
			total++;
			j = certify(&x, cfg.infile, pkey, x509p, dgst,
			    cfg.sigopts, attribs, db, serial,
			    cfg.subj, cfg.chtype,
			    cfg.multirdn, cfg.email_dn,
			    cfg.startdate, cfg.enddate,
			    cfg.days, cfg.batch,
			    cfg.extensions, conf, cfg.verbose,
			    certopt, nameopt, default_op, ext_copy,
			    cfg.selfsign);
			if (j < 0)
				goto err;
			if (j > 0) {
				total_done++;
				BIO_printf(bio_err, "\n");
				if (!BN_add_word(serial, 1))
					goto err;
				if (!sk_X509_push(cert_sk, x)) {
					BIO_printf(bio_err,
					    "Memory allocation failure\n");
					goto err;
				}
			}
		}
		for (i = 0; i < cfg.infiles_num; i++) {
			total++;
			j = certify(&x, cfg.infiles[i], pkey, x509p, dgst,
			    cfg.sigopts, attribs, db, serial,
			    cfg.subj, cfg.chtype,
			    cfg.multirdn, cfg.email_dn,
			    cfg.startdate, cfg.enddate,
			    cfg.days, cfg.batch,
			    cfg.extensions, conf, cfg.verbose,
			    certopt, nameopt, default_op, ext_copy,
			    cfg.selfsign);
			if (j < 0)
				goto err;
			if (j > 0) {
				total_done++;
				BIO_printf(bio_err, "\n");
				if (!BN_add_word(serial, 1))
					goto err;
				if (!sk_X509_push(cert_sk, x)) {
					BIO_printf(bio_err,
					    "Memory allocation failure\n");
					goto err;
				}
			}
		}
		/*
		 * we have a stack of newly certified certificates and a data
		 * base and serial number that need updating
		 */

		if (sk_X509_num(cert_sk) > 0) {
			if (!cfg.batch) {
				char answer[10];

				BIO_printf(bio_err,
				    "\n%d out of %d certificate requests certified, commit? [y/n]",
				    total_done, total);
				(void) BIO_flush(bio_err);
				if (fgets(answer, sizeof answer - 1, stdin) ==
				    NULL) {
					BIO_printf(bio_err,
					    "CERTIFICATION CANCELED: I/O error\n");
					ret = 0;
					goto err;
				}
				if ((answer[0] != 'y') && (answer[0] != 'Y')) {
					BIO_printf(bio_err,
					    "CERTIFICATION CANCELED\n");
					ret = 0;
					goto err;
				}
			}
			BIO_printf(bio_err,
			    "Write out database with %d new entries\n",
			    sk_X509_num(cert_sk));

			if (!save_serial(serialfile, "new", serial, NULL))
				goto err;

			if (!save_index(dbfile, "new", db))
				goto err;
		}
		if (cfg.verbose)
			BIO_printf(bio_err, "writing new certificates\n");
		for (i = 0; i < sk_X509_num(cert_sk); i++) {
			ASN1_INTEGER *serialNumber;
			int k;
			char *serialstr;
			unsigned char *data;
			char pempath[PATH_MAX];

			x = sk_X509_value(cert_sk, i);

			serialNumber = X509_get_serialNumber(x);
			j = ASN1_STRING_length(serialNumber);
			data = ASN1_STRING_data(serialNumber);

			if (j > 0)
				serialstr = bin2hex(data, j);
			else
				serialstr = strdup("00");
			if (serialstr != NULL) {
				k = snprintf(pempath, sizeof(pempath),
				    "%s/%s.pem", cfg.outdir, serialstr);
				free(serialstr);
				if (k < 0 || k >= sizeof(pempath)) {
					BIO_printf(bio_err,
					    "certificate file name too long\n");
					goto err;
				}
			} else {
				BIO_printf(bio_err,
				    "memory allocation failed\n");
				goto err;
			}
			if (cfg.verbose)
				BIO_printf(bio_err, "writing %s\n", pempath);

			if (BIO_write_filename(Cout, pempath) <= 0) {
				perror(pempath);
				goto err;
			}
			if (!write_new_certificate(Cout, x, 0,
			    cfg.notext))
				goto err;
			if (!write_new_certificate(Sout, x, output_der,
			    cfg.notext))
				goto err;
		}

		if (sk_X509_num(cert_sk)) {
			/* Rename the database and the serial file */
			if (!rotate_serial(serialfile, "new", "old"))
				goto err;

			if (!rotate_index(dbfile, "new", "old"))
				goto err;

			BIO_printf(bio_err, "Data Base Updated\n");
		}
	}
	/*****************************************************************/
	if (cfg.gencrl) {
		int crl_v2 = 0;
		if (cfg.crl_ext == NULL) {
			cfg.crl_ext = NCONF_get_string(conf,
			    cfg.section, ENV_CRLEXT);
			if (cfg.crl_ext == NULL)
				ERR_clear_error();
		}
		if (cfg.crl_ext != NULL) {
			/* Check syntax of file */
			X509V3_CTX ctx;
			X509V3_set_ctx_test(&ctx);
			X509V3_set_nconf(&ctx, conf);
			if (!X509V3_EXT_add_nconf(conf, &ctx, cfg.crl_ext,
			    NULL)) {
				BIO_printf(bio_err,
				    "Error Loading CRL extension section %s\n",
				    cfg.crl_ext);
				ret = 1;
				goto err;
			}
		}
		if ((crlnumberfile = NCONF_get_string(conf, cfg.section,
		    ENV_CRLNUMBER)) != NULL)
			if ((crlnumber = load_serial(crlnumberfile, 0,
			    NULL)) == NULL) {
				BIO_printf(bio_err,
				    "error while loading CRL number\n");
				goto err;
			}
		if (!cfg.crldays && !cfg.crlhours &&
		    !cfg.crlsec) {
			if (!NCONF_get_number(conf, cfg.section,
			    ENV_DEFAULT_CRL_DAYS, &cfg.crldays))
				cfg.crldays = 0;
			if (!NCONF_get_number(conf, cfg.section,
			    ENV_DEFAULT_CRL_HOURS, &cfg.crlhours))
				cfg.crlhours = 0;
			ERR_clear_error();
		}
		if ((cfg.crldays == 0) && (cfg.crlhours == 0) &&
		    (cfg.crlsec == 0)) {
			BIO_printf(bio_err,
			    "cannot lookup how long until the next CRL is issued\n");
			goto err;
		}
		if (cfg.verbose)
			BIO_printf(bio_err, "making CRL\n");
		if ((crl = X509_CRL_new()) == NULL)
			goto err;
		if (!X509_CRL_set_issuer_name(crl, X509_get_subject_name(x509)))
			goto err;

		if ((tmptm = X509_gmtime_adj(NULL, 0)) == NULL)
			goto err;
		if (!X509_CRL_set_lastUpdate(crl, tmptm))
			goto err;
		if (X509_time_adj_ex(tmptm, cfg.crldays,
		    cfg.crlhours * 60 * 60 + cfg.crlsec, NULL) ==
		    NULL) {
			BIO_puts(bio_err, "error setting CRL nextUpdate\n");
			goto err;
		}
		if (!X509_CRL_set_nextUpdate(crl, tmptm))
			goto err;
		ASN1_TIME_free(tmptm);
		tmptm = NULL;

		for (i = 0; i < sk_OPENSSL_PSTRING_num(db->db->data); i++) {
			pp = sk_OPENSSL_PSTRING_value(db->db->data, i);
			if (pp[DB_type][0] == DB_TYPE_REV) {
				if ((r = X509_REVOKED_new()) == NULL)
					goto err;
				j = make_revoked(r, pp[DB_rev_date]);
				if (!j)
					goto err;
				if (j == 2)
					crl_v2 = 1;
				if (!BN_hex2bn(&serial, pp[DB_serial]))
					goto err;
				tmpserial = BN_to_ASN1_INTEGER(serial, NULL);
				BN_free(serial);
				serial = NULL;
				if (tmpserial == NULL)
					goto err;
				if (!X509_REVOKED_set_serialNumber(r, tmpserial)) {
					ASN1_INTEGER_free(tmpserial);
					goto err;
				}
				ASN1_INTEGER_free(tmpserial);
				if (!X509_CRL_add0_revoked(crl, r))
					goto err;
				r = NULL;
			}
		}

		/*
		 * sort the data so it will be written in serial number order
		 */
		X509_CRL_sort(crl);

		/* we now have a CRL */
		if (cfg.verbose)
			BIO_printf(bio_err, "signing CRL\n");

		/* Add any extensions asked for */

		if (cfg.crl_ext != NULL || crlnumberfile != NULL) {
			X509V3_CTX crlctx;
			X509V3_set_ctx(&crlctx, x509, NULL, NULL, crl, 0);
			X509V3_set_nconf(&crlctx, conf);

			if (cfg.crl_ext != NULL)
				if (!X509V3_EXT_CRL_add_nconf(conf, &crlctx,
				    cfg.crl_ext, crl))
					goto err;
			if (crlnumberfile != NULL) {
				tmpserial = BN_to_ASN1_INTEGER(crlnumber, NULL);
				if (tmpserial == NULL)
					goto err;
				if (!X509_CRL_add1_ext_i2d(crl, NID_crl_number,
				    tmpserial, 0, 0)) {
					ASN1_INTEGER_free(tmpserial);
					goto err;
				}
				ASN1_INTEGER_free(tmpserial);
				crl_v2 = 1;
				if (!BN_add_word(crlnumber, 1))
					goto err;
			}
		}
		if (cfg.crl_ext != NULL || crl_v2) {
			if (!X509_CRL_set_version(crl, 1))
				goto err;	/* version 2 CRL */
		}
		if (crlnumberfile != NULL)	/* we have a CRL number that
						 * need updating */
			if (!save_serial(crlnumberfile, "new", crlnumber, NULL))
				goto err;

		BN_free(crlnumber);
		crlnumber = NULL;

		if (!do_X509_CRL_sign(bio_err, crl, pkey, dgst,
		    cfg.sigopts))
			goto err;

		if (!PEM_write_bio_X509_CRL(Sout, crl))
			goto err;

		if (crlnumberfile != NULL)	/* Rename the crlnumber file */
			if (!rotate_serial(crlnumberfile, "new", "old"))
				goto err;

	}
	/*****************************************************************/
	if (cfg.dorevoke) {
		if (cfg.infile == NULL) {
			BIO_printf(bio_err, "no input files\n");
			goto err;
		} else {
			X509 *revcert;
			revcert = load_cert(bio_err, cfg.infile,
			    FORMAT_PEM, NULL, cfg.infile);
			if (revcert == NULL)
				goto err;
			j = do_revoke(revcert, db, cfg.rev_type,
			    cfg.rev_arg);
			if (j <= 0)
				goto err;
			X509_free(revcert);

			if (!save_index(dbfile, "new", db))
				goto err;

			if (!rotate_index(dbfile, "new", "old"))
				goto err;

			BIO_printf(bio_err, "Data Base Updated\n");
		}
	}
	/*****************************************************************/
	ret = 0;

 err:
	free(tofree);

	BIO_free_all(Cout);
	BIO_free_all(Sout);
	BIO_free_all(out);
	BIO_free_all(in);

	sk_X509_pop_free(cert_sk, X509_free);

	if (ret)
		ERR_print_errors(bio_err);
	if (free_key)
		free(cfg.key);
	BN_free(serial);
	BN_free(crlnumber);
	free_index(db);
	sk_OPENSSL_STRING_free(cfg.sigopts);
	EVP_PKEY_free(pkey);
	X509_free(x509);
	X509_CRL_free(crl);
	X509_REVOKED_free(r);
	ASN1_TIME_free(tmptm);
	NCONF_free(conf);
	NCONF_free(extconf);
	OBJ_cleanup();

	return (ret);
}

static void
lookup_fail(const char *name, const char *tag)
{
	BIO_printf(bio_err, "variable lookup failed for %s::%s\n", name, tag);
}

static int
certify(X509 **xret, char *infile, EVP_PKEY *pkey, X509 *x509,
    const EVP_MD *dgst, STACK_OF(OPENSSL_STRING) *sigopts,
    STACK_OF(CONF_VALUE) *policy, CA_DB *db, BIGNUM *serial, char *subj,
    unsigned long chtype, int multirdn, int email_dn, char *startdate,
    char *enddate, long days, int batch, char *ext_sect, CONF *lconf,
    int verbose, unsigned long certopt, unsigned long nameopt, int default_op,
    int ext_copy, int selfsign)
{
	X509_REQ *req = NULL;
	BIO *in = NULL;
	EVP_PKEY *pktmp = NULL;
	int ok = -1, i;

	in = BIO_new(BIO_s_file());

	if (BIO_read_filename(in, infile) <= 0) {
		perror(infile);
		goto err;
	}
	if ((req = PEM_read_bio_X509_REQ(in, NULL, NULL, NULL)) == NULL) {
		BIO_printf(bio_err, "Error reading certificate request in %s\n",
		    infile);
		goto err;
	}
	if (verbose) {
		if (!X509_REQ_print(bio_err, req))
			goto err;
	}

	BIO_printf(bio_err, "Check that the request matches the signature\n");

	if (selfsign && !X509_REQ_check_private_key(req, pkey)) {
		BIO_printf(bio_err,
		    "Certificate request and CA private key do not match\n");
		ok = 0;
		goto err;
	}
	if ((pktmp = X509_REQ_get0_pubkey(req)) == NULL) {
		BIO_printf(bio_err, "error unpacking public key\n");
		goto err;
	}
	i = X509_REQ_verify(req, pktmp);
	if (i < 0) {
		ok = 0;
		BIO_printf(bio_err, "Signature verification problems....\n");
		goto err;
	}
	if (i == 0) {
		ok = 0;
		BIO_printf(bio_err,
		    "Signature did not match the certificate request\n");
		goto err;
	} else
		BIO_printf(bio_err, "Signature ok\n");

	ok = do_body(xret, pkey, x509, dgst, sigopts, policy, db, serial,
	    subj, chtype, multirdn, email_dn, startdate, enddate, days, batch,
	    verbose, req, ext_sect, lconf, certopt, nameopt, default_op,
	    ext_copy, selfsign);

 err:
	X509_REQ_free(req);
	BIO_free(in);

	return (ok);
}

static int
certify_cert(X509 **xret, char *infile, EVP_PKEY *pkey, X509 *x509,
    const EVP_MD *dgst, STACK_OF(OPENSSL_STRING) *sigopts,
    STACK_OF(CONF_VALUE) *policy, CA_DB *db, BIGNUM *serial, char *subj,
    unsigned long chtype, int multirdn, int email_dn, char *startdate,
    char *enddate, long days, int batch, char *ext_sect, CONF *lconf,
    int verbose, unsigned long certopt, unsigned long nameopt, int default_op,
    int ext_copy)
{
	X509 *req = NULL;
	X509_REQ *rreq = NULL;
	EVP_PKEY *pktmp = NULL;
	int ok = -1, i;

	if ((req = load_cert(bio_err, infile, FORMAT_PEM, NULL,
	    infile)) == NULL)
		goto err;
	if (verbose) {
		if (!X509_print(bio_err, req))
			goto err;
	}

	BIO_printf(bio_err, "Check that the request matches the signature\n");

	if ((pktmp = X509_get0_pubkey(req)) == NULL) {
		BIO_printf(bio_err, "error unpacking public key\n");
		goto err;
	}
	i = X509_verify(req, pktmp);
	if (i < 0) {
		ok = 0;
		BIO_printf(bio_err, "Signature verification problems....\n");
		goto err;
	}
	if (i == 0) {
		ok = 0;
		BIO_printf(bio_err,
		    "Signature did not match the certificate\n");
		goto err;
	} else
		BIO_printf(bio_err, "Signature ok\n");

	if ((rreq = X509_to_X509_REQ(req, NULL, EVP_md5())) == NULL)
		goto err;

	ok = do_body(xret, pkey, x509, dgst, sigopts, policy, db, serial,
	    subj, chtype, multirdn, email_dn, startdate, enddate, days, batch,
	    verbose, rreq, ext_sect, lconf, certopt, nameopt, default_op,
	    ext_copy, 0);

 err:
	X509_REQ_free(rreq);
	X509_free(req);

	return (ok);
}

static int
do_body(X509 **xret, EVP_PKEY *pkey, X509 *x509, const EVP_MD *dgst,
    STACK_OF(OPENSSL_STRING) *sigopts, STACK_OF(CONF_VALUE) *policy,
    CA_DB *db, BIGNUM *serial, char *subj, unsigned long chtype, int multirdn,
    int email_dn, char *startdate, char *enddate, long days, int batch,
    int verbose, X509_REQ *req, char *ext_sect, CONF *lconf,
    unsigned long certopt, unsigned long nameopt, int default_op,
    int ext_copy, int selfsign)
{
	X509_NAME *name = NULL, *CAname = NULL;
	X509_NAME *subject = NULL, *dn_subject = NULL;
	ASN1_UTCTIME *tm;
	ASN1_STRING *str, *str2;
	ASN1_OBJECT *obj;
	X509 *ret = NULL;
	X509_NAME_ENTRY *ne;
	X509_NAME_ENTRY *tne, *push;
	EVP_PKEY *pktmp;
	int ok = -1, i, j, last, nid;
	const char *p;
	CONF_VALUE *cv;
	OPENSSL_STRING row[DB_NUMBER];
	OPENSSL_STRING *irow = NULL;
	OPENSSL_STRING *rrow = NULL;
	const STACK_OF(X509_EXTENSION) *exts;

	*xret = NULL;

	for (i = 0; i < DB_NUMBER; i++)
		row[i] = NULL;

	if (subj != NULL) {
		X509_NAME *n = parse_name(subj, chtype, multirdn);

		if (n == NULL) {
			ERR_print_errors(bio_err);
			goto err;
		}
		if (!X509_REQ_set_subject_name(req, n)) {
			X509_NAME_free(n);
			goto err;
		}
		X509_NAME_free(n);
	}
	if (default_op)
		BIO_printf(bio_err,
		    "The Subject's Distinguished Name is as follows\n");

	name = X509_REQ_get_subject_name(req);
	for (i = 0; i < X509_NAME_entry_count(name); i++) {
		ne = X509_NAME_get_entry(name, i);
		if (ne == NULL)
			goto err;
		str = X509_NAME_ENTRY_get_data(ne);
		if (str == NULL)
			goto err;
		obj = X509_NAME_ENTRY_get_object(ne);
		if (obj == NULL)
			goto err;

		if (cfg.msie_hack) {
			/* assume all type should be strings */
			nid = OBJ_obj2nid(X509_NAME_ENTRY_get_object(ne));
			if (nid == NID_undef)
				goto err;

			if (str->type == V_ASN1_UNIVERSALSTRING)
				ASN1_UNIVERSALSTRING_to_string(str);

			if ((str->type == V_ASN1_IA5STRING) &&
			    (nid != NID_pkcs9_emailAddress))
				str->type = V_ASN1_T61STRING;

			if ((nid == NID_pkcs9_emailAddress) &&
			    (str->type == V_ASN1_PRINTABLESTRING))
				str->type = V_ASN1_IA5STRING;
		}
		/* If no EMAIL is wanted in the subject */
		if ((OBJ_obj2nid(obj) == NID_pkcs9_emailAddress) && (!email_dn))
			continue;

		/* check some things */
		if ((OBJ_obj2nid(obj) == NID_pkcs9_emailAddress) &&
		    (str->type != V_ASN1_IA5STRING)) {
			BIO_printf(bio_err,
			    "\nemailAddress type needs to be of type IA5STRING\n");
			goto err;
		}
		if ((str->type != V_ASN1_BMPSTRING) &&
		    (str->type != V_ASN1_UTF8STRING)) {
			j = ASN1_PRINTABLE_type(str->data, str->length);
			if (((j == V_ASN1_T61STRING) &&
			    (str->type != V_ASN1_T61STRING)) ||
			    ((j == V_ASN1_IA5STRING) &&
			    (str->type == V_ASN1_PRINTABLESTRING))) {
				BIO_printf(bio_err,
				    "\nThe string contains characters that are illegal for the ASN.1 type\n");
				goto err;
			}
		}
		if (default_op)
			old_entry_print(bio_err, obj, str);
	}

	/* Ok, now we check the 'policy' stuff. */
	if ((subject = X509_NAME_new()) == NULL) {
		BIO_printf(bio_err, "Memory allocation failure\n");
		goto err;
	}
	/* take a copy of the issuer name before we mess with it. */
	if (selfsign)
		CAname = X509_NAME_dup(name);
	else
		CAname = X509_NAME_dup(X509_get_subject_name(x509));
	if (CAname == NULL)
		goto err;
	str = str2 = NULL;

	for (i = 0; i < sk_CONF_VALUE_num(policy); i++) {
		cv = sk_CONF_VALUE_value(policy, i);	/* get the object id */
		if ((j = OBJ_txt2nid(cv->name)) == NID_undef) {
			BIO_printf(bio_err,
			    "%s:unknown object type in 'policy' configuration\n",
			    cv->name);
			goto err;
		}
		obj = OBJ_nid2obj(j);
		if (obj == NULL)
			goto err;

		last = -1;
		for (;;) {
			/* lookup the object in the supplied name list */
			j = X509_NAME_get_index_by_OBJ(name, obj, last);
			if (j < 0) {
				if (last != -1)
					break;
				tne = NULL;
			} else {
				tne = X509_NAME_get_entry(name, j);
				if (tne == NULL)
					goto err;
			}
			last = j;

			/* depending on the 'policy', decide what to do. */
			push = NULL;
			if (strcmp(cv->value, "optional") == 0) {
				if (tne != NULL)
					push = tne;
			} else if (strcmp(cv->value, "supplied") == 0) {
				if (tne == NULL) {
					BIO_printf(bio_err,
					    "The %s field needed to be supplied and was missing\n",
					    cv->name);
					goto err;
				} else
					push = tne;
			} else if (strcmp(cv->value, "match") == 0) {
				int last2;

				if (tne == NULL) {
					BIO_printf(bio_err,
					    "The mandatory %s field was missing\n",
					    cv->name);
					goto err;
				}
				last2 = -1;

 again2:
				j = X509_NAME_get_index_by_OBJ(CAname, obj,
				    last2);
				if ((j < 0) && (last2 == -1)) {
					BIO_printf(bio_err,
					    "The %s field does not exist in the CA certificate,\nthe 'policy' is misconfigured\n",
					    cv->name);
					goto err;
				}
				if (j >= 0) {
					push = X509_NAME_get_entry(CAname, j);
					if (push == NULL)
						goto err;
					str = X509_NAME_ENTRY_get_data(tne);
					if (str == NULL)
						goto err;
					str2 = X509_NAME_ENTRY_get_data(push);
					if (str2 == NULL)
						goto err;
					last2 = j;
					if (ASN1_STRING_cmp(str, str2) != 0)
						goto again2;
				}
				if (j < 0) {
					BIO_printf(bio_err,
					    "The %s field needed to be the same in the\nCA certificate (%s) and the request (%s)\n",
					    cv->name, ((str2 == NULL) ?
					    "NULL" : (char *) str2->data),
					    ((str == NULL) ?
					    "NULL" : (char *) str->data));
					goto err;
				}
			} else {
				BIO_printf(bio_err,
				    "%s:invalid type in 'policy' configuration\n",
				    cv->value);
				goto err;
			}

			if (push != NULL) {
				if (!X509_NAME_add_entry(subject, push,
				    -1, 0)) {
					X509_NAME_ENTRY_free(push);
					BIO_printf(bio_err,
					    "Memory allocation failure\n");
					goto err;
				}
			}
			if (j < 0)
				break;
		}
	}

	if (cfg.preserve) {
		X509_NAME_free(subject);
		/* subject=X509_NAME_dup(X509_REQ_get_subject_name(req)); */
		subject = X509_NAME_dup(name);
		if (subject == NULL)
			goto err;
	}

	/* We are now totally happy, lets make and sign the certificate */
	if (verbose)
		BIO_printf(bio_err,
		    "Everything appears to be ok, creating and signing the certificate\n");

	if ((ret = X509_new()) == NULL)
		goto err;

#ifdef X509_V3
	/* Make it an X509 v3 certificate. */
	if (!X509_set_version(ret, 2))
		goto err;
#endif
	if (X509_get_serialNumber(ret) == NULL)
		goto err;
	if (BN_to_ASN1_INTEGER(serial, X509_get_serialNumber(ret)) == NULL)
		goto err;
	if (selfsign) {
		if (!X509_set_issuer_name(ret, subject))
			goto err;
	} else {
		if (!X509_set_issuer_name(ret, X509_get_subject_name(x509)))
			goto err;
	}

	if (strcmp(startdate, "today") == 0) {
		if (X509_gmtime_adj(X509_get_notBefore(ret), 0) == NULL)
			goto err;
	} else if (!ASN1_TIME_set_string_X509(X509_get_notBefore(ret), startdate)) {
		BIO_printf(bio_err, "Invalid start date %s\n", startdate);
		goto err;
	}

	if (enddate == NULL) {
		if (X509_time_adj_ex(X509_get_notAfter(ret), days, 0,
		    NULL) == NULL)
			goto err;
	} else if (!ASN1_TIME_set_string_X509(X509_get_notAfter(ret), enddate)) {
		BIO_printf(bio_err, "Invalid end date %s\n", enddate);
		goto err;
	}

	if (!X509_set_subject_name(ret, subject))
		goto err;

	if ((pktmp = X509_REQ_get0_pubkey(req)) == NULL)
		goto err;

	if (!X509_set_pubkey(ret, pktmp))
		goto err;

	/* Lets add the extensions, if there are any */
	if (ext_sect != NULL) {
		X509V3_CTX ctx;

		/* Initialize the context structure */
		if (selfsign)
			X509V3_set_ctx(&ctx, ret, ret, req, NULL, 0);
		else
			X509V3_set_ctx(&ctx, x509, ret, req, NULL, 0);

		if (extconf != NULL) {
			if (verbose)
				BIO_printf(bio_err,
				    "Extra configuration file found\n");

			/* Use the extconf configuration db LHASH */
			X509V3_set_nconf(&ctx, extconf);

			/* Test the structure (needed?) */
			/* X509V3_set_ctx_test(&ctx); */

			/* Adds exts contained in the configuration file */
			if (!X509V3_EXT_add_nconf(extconf, &ctx,
			    ext_sect, ret)) {
				BIO_printf(bio_err,
				    "ERROR: adding extensions in section %s\n",
				    ext_sect);
				ERR_print_errors(bio_err);
				goto err;
			}
			if (verbose)
				BIO_printf(bio_err,
				    "Successfully added extensions from file.\n");
		} else if (ext_sect != NULL) {
			/* We found extensions to be set from config file */
			X509V3_set_nconf(&ctx, lconf);

			if (!X509V3_EXT_add_nconf(lconf, &ctx, ext_sect, ret)) {
				BIO_printf(bio_err,
				    "ERROR: adding extensions in section %s\n",
				    ext_sect);
				ERR_print_errors(bio_err);
				goto err;
			}
			if (verbose)
				BIO_printf(bio_err,
				    "Successfully added extensions from config\n");
		}
	}

	/* Copy extensions from request (if any) */
	if (!copy_extensions(ret, req, ext_copy)) {
		BIO_printf(bio_err, "ERROR: adding extensions from request\n");
		ERR_print_errors(bio_err);
		goto err;
	}

	exts = X509_get0_extensions(ret);
	if (exts != NULL && sk_X509_EXTENSION_num(exts) > 0) {
		/* Make it an X509 v3 certificate. */
		if (!X509_set_version(ret, 2))
			goto err;
	}

	if (verbose)
		BIO_printf(bio_err,
		    "The subject name appears to be ok, checking data base for clashes\n");

	/* Build the correct Subject if no email is wanted in the subject */
	if (!email_dn) {
		X509_NAME_ENTRY *tmpne;
		/*
		 * Its best to dup the subject DN and then delete any email
		 * addresses because this retains its structure.
		 */
		if ((dn_subject = X509_NAME_dup(subject)) == NULL) {
			BIO_printf(bio_err, "Memory allocation failure\n");
			goto err;
		}
		while ((i = X509_NAME_get_index_by_NID(dn_subject,
		    NID_pkcs9_emailAddress, -1)) >= 0) {
			tmpne = X509_NAME_get_entry(dn_subject, i);
			if (tmpne == NULL)
				goto err;
			if (X509_NAME_delete_entry(dn_subject, i) == NULL) {
				X509_NAME_ENTRY_free(tmpne);
				goto err;
			}
			X509_NAME_ENTRY_free(tmpne);
		}

		if (!X509_set_subject_name(ret, dn_subject))
			goto err;

		X509_NAME_free(dn_subject);
		dn_subject = NULL;
	}

	row[DB_name] = X509_NAME_oneline(X509_get_subject_name(ret), NULL, 0);
	if (row[DB_name] == NULL) {
		BIO_printf(bio_err, "Memory allocation failure\n");
		goto err;
	}

	if (BN_is_zero(serial))
		row[DB_serial] = strdup("00");
	else
		row[DB_serial] = BN_bn2hex(serial);
	if (row[DB_serial] == NULL) {
		BIO_printf(bio_err, "Memory allocation failure\n");
		goto err;
	}

	if (row[DB_name][0] == '\0') {
		/*
		 * An empty subject! We'll use the serial number instead. If
		 * unique_subject is in use then we don't want different
		 * entries with empty subjects matching each other.
		 */
		free(row[DB_name]);
		row[DB_name] = strdup(row[DB_serial]);
		if (row[DB_name] == NULL) {
			BIO_printf(bio_err, "Memory allocation failure\n");
			goto err;
		}
	}

	if (db->attributes.unique_subject) {
		OPENSSL_STRING *crow = row;

		rrow = TXT_DB_get_by_index(db->db, DB_name, crow);
		if (rrow != NULL) {
			BIO_printf(bio_err,
			    "ERROR:There is already a certificate for %s\n",
			    row[DB_name]);
		}
	}
	if (rrow == NULL) {
		rrow = TXT_DB_get_by_index(db->db, DB_serial, row);
		if (rrow != NULL) {
			BIO_printf(bio_err,
			    "ERROR:Serial number %s has already been issued,\n",
			    row[DB_serial]);
			BIO_printf(bio_err,
			    "      check the database/serial_file for corruption\n");
		}
	}
	if (rrow != NULL) {
		BIO_printf(bio_err,
		    "The matching entry has the following details\n");
		if (rrow[DB_type][0] == DB_TYPE_EXP)
			p = "Expired";
		else if (rrow[DB_type][0] == DB_TYPE_REV)
			p = "Revoked";
		else if (rrow[DB_type][0] == DB_TYPE_VAL)
			p = "Valid";
		else
			p = "\ninvalid type, Data base error\n";
		BIO_printf(bio_err, "Type	  :%s\n", p);
		if (rrow[DB_type][0] == DB_TYPE_REV) {
			p = rrow[DB_exp_date];
			if (p == NULL)
				p = "undef";
			BIO_printf(bio_err, "Was revoked on:%s\n", p);
		}
		p = rrow[DB_exp_date];
		if (p == NULL)
			p = "undef";
		BIO_printf(bio_err, "Expires on    :%s\n", p);
		p = rrow[DB_serial];
		if (p == NULL)
			p = "undef";
		BIO_printf(bio_err, "Serial Number :%s\n", p);
		p = rrow[DB_file];
		if (p == NULL)
			p = "undef";
		BIO_printf(bio_err, "File name     :%s\n", p);
		p = rrow[DB_name];
		if (p == NULL)
			p = "undef";
		BIO_printf(bio_err, "Subject Name  :%s\n", p);
		ok = -1;	/* This is now a 'bad' error. */
		goto err;
	}

	if (!default_op) {
		BIO_printf(bio_err, "Certificate Details:\n");
		/*
		 * Never print signature details because signature not
		 * present
		 */
		certopt |= X509_FLAG_NO_SIGDUMP | X509_FLAG_NO_SIGNAME;
		if (!X509_print_ex(bio_err, ret, nameopt, certopt))
			goto err;
	}
	BIO_printf(bio_err, "Certificate is to be certified until ");
	ASN1_TIME_print(bio_err, X509_get_notAfter(ret));
	if (days)
		BIO_printf(bio_err, " (%ld days)", days);
	BIO_printf(bio_err, "\n");

	if (!batch) {
		char answer[25];

		BIO_printf(bio_err, "Sign the certificate? [y/n]:");
		(void) BIO_flush(bio_err);
		if (!fgets(answer, sizeof(answer) - 1, stdin)) {
			BIO_printf(bio_err,
			    "CERTIFICATE WILL NOT BE CERTIFIED: I/O error\n");
			ok = 0;
			goto err;
		}
		if (!((answer[0] == 'y') || (answer[0] == 'Y'))) {
			BIO_printf(bio_err,
			    "CERTIFICATE WILL NOT BE CERTIFIED\n");
			ok = 0;
			goto err;
		}
	}

	if ((pktmp = X509_get0_pubkey(ret)) == NULL)
		goto err;

	if (EVP_PKEY_missing_parameters(pktmp) &&
	    !EVP_PKEY_missing_parameters(pkey)) {
		if (!EVP_PKEY_copy_parameters(pktmp, pkey)) {
			goto err;
		}
	}

	if (!do_X509_sign(bio_err, ret, pkey, dgst, sigopts))
		goto err;

	/* We now just add it to the database */
	row[DB_type] = malloc(2);

	if ((tm = X509_get_notAfter(ret)) == NULL)
		goto err;
	row[DB_exp_date] = strndup(tm->data, tm->length);
	if (row[DB_type] == NULL || row[DB_exp_date] == NULL) {
		BIO_printf(bio_err, "Memory allocation failure\n");
		goto err;
	}

	row[DB_rev_date] = NULL;

	/* row[DB_serial] done already */
	row[DB_file] = malloc(8);

	if ((row[DB_type] == NULL) || (row[DB_file] == NULL) ||
	    (row[DB_name] == NULL)) {
		BIO_printf(bio_err, "Memory allocation failure\n");
		goto err;
	}
	(void) strlcpy(row[DB_file], "unknown", 8);
	row[DB_type][0] = DB_TYPE_VAL;
	row[DB_type][1] = '\0';

	if ((irow = reallocarray(NULL, DB_NUMBER + 1, sizeof(char *))) ==
	    NULL) {
		BIO_printf(bio_err, "Memory allocation failure\n");
		goto err;
	}
	for (i = 0; i < DB_NUMBER; i++) {
		irow[i] = row[i];
		row[i] = NULL;
	}
	irow[DB_NUMBER] = NULL;

	if (!TXT_DB_insert(db->db, irow)) {
		BIO_printf(bio_err, "failed to update database\n");
		BIO_printf(bio_err, "TXT_DB error number %ld\n", db->db->error);
		goto err;
	}

	*xret = ret;
	ret = NULL;
	ok = 1;

 err:
	for (i = 0; i < DB_NUMBER; i++)
		free(row[i]);

	X509_NAME_free(CAname);
	X509_NAME_free(subject);
	X509_NAME_free(dn_subject);
	X509_free(ret);

	return (ok);
}

static int
write_new_certificate(BIO *bp, X509 *x, int output_der, int notext)
{
	if (output_der) {
		if (!i2d_X509_bio(bp, x))
			return (0);
	}
	if (!notext) {
		if (!X509_print(bp, x))
			return (0);
	}

	return PEM_write_bio_X509(bp, x);
}

static int
certify_spkac(X509 **xret, char *infile, EVP_PKEY *pkey, X509 *x509,
    const EVP_MD *dgst, STACK_OF(OPENSSL_STRING) *sigopts,
    STACK_OF(CONF_VALUE) *policy, CA_DB *db, BIGNUM *serial, char *subj,
    unsigned long chtype, int multirdn, int email_dn, char *startdate,
    char *enddate, long days, char *ext_sect, CONF *lconf, int verbose,
    unsigned long certopt, unsigned long nameopt, int default_op, int ext_copy)
{
	STACK_OF(CONF_VALUE) *sk = NULL;
	LHASH_OF(CONF_VALUE) *parms = NULL;
	X509_REQ *req = NULL;
	CONF_VALUE *cv = NULL;
	NETSCAPE_SPKI *spki = NULL;
	char *type, *buf;
	EVP_PKEY *pktmp = NULL;
	X509_NAME *n = NULL;
	int ok = -1, i, j;
	long errline;
	int nid;

	/*
	 * Load input file into a hash table.  (This is just an easy
	 * way to read and parse the file, then put it into a convenient
	 * STACK format).
	 */
	parms = CONF_load(NULL, infile, &errline);
	if (parms == NULL) {
		BIO_printf(bio_err, "error on line %ld of %s\n",
		    errline, infile);
		ERR_print_errors(bio_err);
		goto err;
	}
	sk = CONF_get_section(parms, "default");
	if (sk_CONF_VALUE_num(sk) == 0) {
		BIO_printf(bio_err, "no name/value pairs found in %s\n",
		    infile);
		CONF_free(parms);
		goto err;
	}
	/*
	 * Now create a dummy X509 request structure.  We don't actually
	 * have an X509 request, but we have many of the components
	 * (a public key, various DN components).  The idea is that we
	 * put these components into the right X509 request structure
	 * and we can use the same code as if you had a real X509 request.
	 */
	req = X509_REQ_new();
	if (req == NULL) {
		ERR_print_errors(bio_err);
		goto err;
	}
	/*
	 * Build up the subject name set.
	 */
	n = X509_REQ_get_subject_name(req);

	for (i = 0;; i++) {
		if (sk_CONF_VALUE_num(sk) <= i)
			break;

		cv = sk_CONF_VALUE_value(sk, i);
		type = cv->name;
		/*
		 * Skip past any leading X. X: X, etc to allow for multiple
		 * instances
		 */
		for (buf = cv->name; *buf; buf++) {
			if ((*buf == ':') || (*buf == ',') || (*buf == '.')) {
				buf++;
				if (*buf)
					type = buf;
				break;
			}
		}

		buf = cv->value;
		if ((nid = OBJ_txt2nid(type)) == NID_undef) {
			if (strcmp(type, "SPKAC") == 0) {
				spki = NETSCAPE_SPKI_b64_decode(cv->value, -1);
				if (spki == NULL) {
					BIO_printf(bio_err,
					    "unable to load Netscape SPKAC structure\n");
					ERR_print_errors(bio_err);
					goto err;
				}
			}
			continue;
		}
		if (!X509_NAME_add_entry_by_NID(n, nid, chtype,
		    (unsigned char *)buf, -1, -1, 0))
			goto err;
	}
	if (spki == NULL) {
		BIO_printf(bio_err,
		    "Netscape SPKAC structure not found in %s\n", infile);
		goto err;
	}
	/*
	 * Now extract the key from the SPKI structure.
	 */

	BIO_printf(bio_err,
	    "Check that the SPKAC request matches the signature\n");

	if ((pktmp = NETSCAPE_SPKI_get_pubkey(spki)) == NULL) {
		BIO_printf(bio_err, "error unpacking SPKAC public key\n");
		goto err;
	}
	j = NETSCAPE_SPKI_verify(spki, pktmp);
	if (j <= 0) {
		BIO_printf(bio_err,
		    "signature verification failed on SPKAC public key\n");
		goto err;
	}
	BIO_printf(bio_err, "Signature ok\n");

	if (!X509_REQ_set_pubkey(req, pktmp)) {
		EVP_PKEY_free(pktmp);
		goto err;
	}
	EVP_PKEY_free(pktmp);
	ok = do_body(xret, pkey, x509, dgst, sigopts, policy, db, serial,
	    subj, chtype, multirdn, email_dn, startdate, enddate, days, 1,
	    verbose, req, ext_sect, lconf, certopt, nameopt, default_op,
	    ext_copy, 0);

 err:
	X509_REQ_free(req);
	CONF_free(parms);
	NETSCAPE_SPKI_free(spki);

	return (ok);
}

static int
check_time_format(const char *str)
{
	return ASN1_TIME_set_string(NULL, str);
}

static int
do_revoke(X509 *x509, CA_DB *db, int type, char *value)
{
	ASN1_UTCTIME *tm = NULL;
	char *row[DB_NUMBER], **rrow, **irow;
	char *rev_str = NULL;
	BIGNUM *bn = NULL;
	int ok = -1, i;

	for (i = 0; i < DB_NUMBER; i++)
		row[i] = NULL;
	row[DB_name] = X509_NAME_oneline(X509_get_subject_name(x509), NULL, 0);
	bn = ASN1_INTEGER_to_BN(X509_get_serialNumber(x509), NULL);
	if (bn == NULL)
		goto err;
	if (BN_is_zero(bn))
		row[DB_serial] = strdup("00");
	else
		row[DB_serial] = BN_bn2hex(bn);
	BN_free(bn);

	if (row[DB_name] != NULL && row[DB_name][0] == '\0') {
		/*
		 * Entries with empty Subjects actually use the serial number
		 * instead
		 */
		free(row[DB_name]);
		row[DB_name] = strdup(row[DB_serial]);
		if (row[DB_name] == NULL) {
			BIO_printf(bio_err, "Memory allocation failure\n");
			goto err;
		}
	}

	if ((row[DB_name] == NULL) || (row[DB_serial] == NULL)) {
		BIO_printf(bio_err, "Memory allocation failure\n");
		goto err;
	}
	/*
	 * We have to lookup by serial number because name lookup skips
	 * revoked certs
	 */
	rrow = TXT_DB_get_by_index(db->db, DB_serial, row);
	if (rrow == NULL) {
		BIO_printf(bio_err,
		    "Adding Entry with serial number %s to DB for %s\n",
		    row[DB_serial], row[DB_name]);

		/* We now just add it to the database */
		row[DB_type] = malloc(2);

		if ((tm = X509_get_notAfter(x509)) == NULL)
			goto err;
		row[DB_exp_date] = strndup(tm->data, tm->length);
		if (row[DB_type] == NULL || row[DB_exp_date] == NULL) {
			BIO_printf(bio_err, "Memory allocation failure\n");
			goto err;
		}

		row[DB_rev_date] = NULL;

		/* row[DB_serial] done already */
		row[DB_file] = malloc(8);

		/* row[DB_name] done already */

		if ((row[DB_type] == NULL) || (row[DB_file] == NULL)) {
			BIO_printf(bio_err, "Memory allocation failure\n");
			goto err;
		}
		(void) strlcpy(row[DB_file], "unknown", 8);
		row[DB_type][0] = DB_TYPE_VAL;
		row[DB_type][1] = '\0';

		if ((irow = reallocarray(NULL, sizeof(char *),
		    (DB_NUMBER + 1))) == NULL) {
			BIO_printf(bio_err, "Memory allocation failure\n");
			goto err;
		}
		for (i = 0; i < DB_NUMBER; i++) {
			irow[i] = row[i];
			row[i] = NULL;
		}
		irow[DB_NUMBER] = NULL;

		if (!TXT_DB_insert(db->db, irow)) {
			BIO_printf(bio_err, "failed to update database\n");
			BIO_printf(bio_err, "TXT_DB error number %ld\n",
			    db->db->error);
			goto err;
		}
		/* Revoke Certificate */
		ok = do_revoke(x509, db, type, value);

		goto err;

	} else if (index_name_cmp_noconst(row, rrow)) {
		BIO_printf(bio_err, "ERROR:name does not match %s\n",
		    row[DB_name]);
		goto err;
	} else if (rrow[DB_type][0] == DB_TYPE_REV) {
		BIO_printf(bio_err, "ERROR:Already revoked, serial number %s\n",
		    row[DB_serial]);
		goto err;
	} else {
		BIO_printf(bio_err, "Revoking Certificate %s.\n",
		    rrow[DB_serial]);
		rev_str = make_revocation_str(type, value);
		if (rev_str == NULL) {
			BIO_printf(bio_err, "Error in revocation arguments\n");
			goto err;
		}
		rrow[DB_type][0] = DB_TYPE_REV;
		rrow[DB_type][1] = '\0';
		rrow[DB_rev_date] = rev_str;
	}
	ok = 1;

 err:
	for (i = 0; i < DB_NUMBER; i++)
		free(row[i]);

	return (ok);
}

static int
get_certificate_status(const char *serial, CA_DB *db)
{
	char *row[DB_NUMBER], **rrow;
	int ok = -1, i;

	/* Free Resources */
	for (i = 0; i < DB_NUMBER; i++)
		row[i] = NULL;

	/* Malloc needed char spaces */
	row[DB_serial] = malloc(strlen(serial) + 2);
	if (row[DB_serial] == NULL) {
		BIO_printf(bio_err, "Malloc failure\n");
		goto err;
	}
	if (strlen(serial) % 2) {
		row[DB_serial][0] = '0';

		/* Copy String from serial to row[DB_serial] */
		memcpy(row[DB_serial] + 1, serial, strlen(serial));
		row[DB_serial][strlen(serial) + 1] = '\0';
	} else {
		/* Copy String from serial to row[DB_serial] */
		memcpy(row[DB_serial], serial, strlen(serial));
		row[DB_serial][strlen(serial)] = '\0';
	}

	/* Make it Upper Case */
	for (i = 0; row[DB_serial][i] != '\0'; i++)
		row[DB_serial][i] = toupper((unsigned char) row[DB_serial][i]);


	ok = 1;

	/* Search for the certificate */
	rrow = TXT_DB_get_by_index(db->db, DB_serial, row);
	if (rrow == NULL) {
		BIO_printf(bio_err, "Serial %s not present in db.\n",
		    row[DB_serial]);
		ok = -1;
		goto err;
	} else if (rrow[DB_type][0] == DB_TYPE_VAL) {
		BIO_printf(bio_err, "%s=Valid (%c)\n",
		    row[DB_serial], rrow[DB_type][0]);
		goto err;
	} else if (rrow[DB_type][0] == DB_TYPE_REV) {
		BIO_printf(bio_err, "%s=Revoked (%c)\n",
		    row[DB_serial], rrow[DB_type][0]);
		goto err;
	} else if (rrow[DB_type][0] == DB_TYPE_EXP) {
		BIO_printf(bio_err, "%s=Expired (%c)\n",
		    row[DB_serial], rrow[DB_type][0]);
		goto err;
	} else if (rrow[DB_type][0] == DB_TYPE_SUSP) {
		BIO_printf(bio_err, "%s=Suspended (%c)\n",
		    row[DB_serial], rrow[DB_type][0]);
		goto err;
	} else {
		BIO_printf(bio_err, "%s=Unknown (%c).\n",
		    row[DB_serial], rrow[DB_type][0]);
		ok = -1;
	}

 err:
	for (i = 0; i < DB_NUMBER; i++)
		free(row[i]);

	return (ok);
}

static int
do_updatedb(CA_DB *db)
{
	ASN1_UTCTIME *a_tm = NULL;
	int i, cnt = 0;
	int db_y2k, a_y2k;	/* flags = 1 if y >= 2000 */
	char **rrow, *a_tm_s = NULL;

	a_tm = ASN1_UTCTIME_new();
	if (a_tm == NULL) {
		cnt = -1;
		goto err;
	}

	/* get actual time and make a string */
	a_tm = X509_gmtime_adj(a_tm, 0);
	if (a_tm == NULL) {
		cnt = -1;
		goto err;
	}
	a_tm_s = strndup(a_tm->data, a_tm->length);
	if (a_tm_s == NULL) {
		cnt = -1;
		goto err;
	}

	if (strncmp(a_tm_s, "49", 2) <= 0)
		a_y2k = 1;
	else
		a_y2k = 0;

	for (i = 0; i < sk_OPENSSL_PSTRING_num(db->db->data); i++) {
		rrow = sk_OPENSSL_PSTRING_value(db->db->data, i);

		if (rrow[DB_type][0] == DB_TYPE_VAL) {
			/* ignore entries that are not valid */
			if (strncmp(rrow[DB_exp_date], "49", 2) <= 0)
				db_y2k = 1;
			else
				db_y2k = 0;

			if (db_y2k == a_y2k) {
				/* all on the same y2k side */
				if (strcmp(rrow[DB_exp_date], a_tm_s) <= 0) {
					rrow[DB_type][0] = DB_TYPE_EXP;
					rrow[DB_type][1] = '\0';
					cnt++;

					BIO_printf(bio_err, "%s=Expired\n",
					    rrow[DB_serial]);
				}
			} else if (db_y2k < a_y2k) {
				rrow[DB_type][0] = DB_TYPE_EXP;
				rrow[DB_type][1] = '\0';
				cnt++;

				BIO_printf(bio_err, "%s=Expired\n",
				    rrow[DB_serial]);
			}
		}
	}

 err:
	ASN1_UTCTIME_free(a_tm);
	free(a_tm_s);

	return (cnt);
}

static const char *crl_reasons[] = {
	/* CRL reason strings */
	"unspecified",
	"keyCompromise",
	"CACompromise",
	"affiliationChanged",
	"superseded",
	"cessationOfOperation",
	"certificateHold",
	"removeFromCRL",
	/* Additional pseudo reasons */
	"holdInstruction",
	"keyTime",
	"CAkeyTime"
};

#define NUM_REASONS (sizeof(crl_reasons) / sizeof(char *))

/* Given revocation information convert to a DB string.
 * The format of the string is:
 * revtime[,reason,extra]. Where 'revtime' is the
 * revocation time (the current time). 'reason' is the
 * optional CRL reason and 'extra' is any additional
 * argument
 */

char *
make_revocation_str(int rev_type, char *rev_arg)
{
	char *other = NULL, *str;
	const char *reason = NULL;
	ASN1_OBJECT *otmp;
	ASN1_UTCTIME *revtm = NULL;
	int i;
	switch (rev_type) {
	case REV_NONE:
		break;

	case REV_CRL_REASON:
		for (i = 0; i < 8; i++) {
			if (strcasecmp(rev_arg, crl_reasons[i]) == 0) {
				reason = crl_reasons[i];
				break;
			}
		}
		if (reason == NULL) {
			BIO_printf(bio_err, "Unknown CRL reason %s\n", rev_arg);
			return NULL;
		}
		break;

	case REV_HOLD:
		/* Argument is an OID */
		otmp = OBJ_txt2obj(rev_arg, 0);
		ASN1_OBJECT_free(otmp);

		if (otmp == NULL) {
			BIO_printf(bio_err,
			    "Invalid object identifier %s\n", rev_arg);
			return NULL;
		}
		reason = "holdInstruction";
		other = rev_arg;
		break;

	case REV_KEY_COMPROMISE:
	case REV_CA_COMPROMISE:
		/* Argument is the key compromise time  */
		if (!ASN1_GENERALIZEDTIME_set_string(NULL, rev_arg)) {
			BIO_printf(bio_err,
			    "Invalid time format %s. Need YYYYMMDDHHMMSSZ\n",
			    rev_arg);
			return NULL;
		}
		other = rev_arg;
		if (rev_type == REV_KEY_COMPROMISE)
			reason = "keyTime";
		else
			reason = "CAkeyTime";

		break;
	}

	revtm = X509_gmtime_adj(NULL, 0);
	if (revtm == NULL)
		return NULL;

	if (asprintf(&str, "%s%s%s%s%s", revtm->data,
	    reason ? "," : "", reason ? reason : "",
	    other ? "," : "", other ? other : "") == -1)
		str = NULL;

	ASN1_UTCTIME_free(revtm);

	return str;
}

/* Convert revocation field to X509_REVOKED entry
 * return code:
 * 0 error
 * 1 OK
 * 2 OK and some extensions added (i.e. V2 CRL)
 */

int
make_revoked(X509_REVOKED *rev, const char *str)
{
	char *tmp = NULL;
	int reason_code = -1;
	int i, ret = 0;
	ASN1_OBJECT *hold = NULL;
	ASN1_GENERALIZEDTIME *comp_time = NULL;
	ASN1_ENUMERATED *rtmp = NULL;

	ASN1_TIME *revDate = NULL;

	i = unpack_revinfo(&revDate, &reason_code, &hold, &comp_time, str);

	if (i == 0)
		goto err;

	if (rev != NULL && !X509_REVOKED_set_revocationDate(rev, revDate))
		goto err;

	if (rev != NULL && (reason_code != OCSP_REVOKED_STATUS_NOSTATUS)) {
		rtmp = ASN1_ENUMERATED_new();
		if (rtmp == NULL || !ASN1_ENUMERATED_set(rtmp, reason_code))
			goto err;
		if (!X509_REVOKED_add1_ext_i2d(rev, NID_crl_reason, rtmp, 0, 0))
			goto err;
	}
	if (rev != NULL && comp_time != NULL) {
		if (!X509_REVOKED_add1_ext_i2d(rev, NID_invalidity_date,
		    comp_time, 0, 0))
			goto err;
	}
	if (rev != NULL && hold != NULL) {
		if (!X509_REVOKED_add1_ext_i2d(rev, NID_hold_instruction_code,
		    hold, 0, 0))
			goto err;
	}
	if (reason_code != OCSP_REVOKED_STATUS_NOSTATUS)
		ret = 2;
	else
		ret = 1;

 err:
	free(tmp);

	ASN1_OBJECT_free(hold);
	ASN1_GENERALIZEDTIME_free(comp_time);
	ASN1_ENUMERATED_free(rtmp);
	ASN1_TIME_free(revDate);

	return ret;
}

int
old_entry_print(BIO *bp, ASN1_OBJECT *obj, ASN1_STRING *str)
{
	char buf[25], *pbuf, *p;
	int j;

	j = i2a_ASN1_OBJECT(bp, obj);
	pbuf = buf;
	for (j = 22 - j; j > 0; j--)
		*(pbuf++) = ' ';
	*(pbuf++) = ':';
	*(pbuf++) = '\0';
	BIO_puts(bp, buf);

	if (str->type == V_ASN1_PRINTABLESTRING)
		BIO_printf(bp, "PRINTABLE:'");
	else if (str->type == V_ASN1_T61STRING)
		BIO_printf(bp, "T61STRING:'");
	else if (str->type == V_ASN1_IA5STRING)
		BIO_printf(bp, "IA5STRING:'");
	else if (str->type == V_ASN1_UNIVERSALSTRING)
		BIO_printf(bp, "UNIVERSALSTRING:'");
	else
		BIO_printf(bp, "ASN.1 %2d:'", str->type);

	p = (char *) str->data;
	for (j = str->length; j > 0; j--) {
		if ((*p >= ' ') && (*p <= '~'))
			BIO_printf(bp, "%c", *p);
		else if (*p & 0x80)
			BIO_printf(bp, "\\0x%02X", *p);
		else if ((unsigned char) *p == 0xf7)
			BIO_printf(bp, "^?");
		else
			BIO_printf(bp, "^%c", *p + '@');
		p++;
	}
	BIO_printf(bp, "'\n");
	return 1;
}

int
unpack_revinfo(ASN1_TIME **prevtm, int *preason, ASN1_OBJECT **phold,
    ASN1_GENERALIZEDTIME **pinvtm, const char *str)
{
	char *tmp = NULL;
	char *rtime_str, *reason_str = NULL, *arg_str = NULL, *p;
	int reason_code = -1;
	int ret = 0;
	unsigned int i;
	ASN1_OBJECT *hold = NULL;
	ASN1_GENERALIZEDTIME *comp_time = NULL;

	if ((tmp = strdup(str)) == NULL) {
		BIO_printf(bio_err, "malloc failed\n");
		goto err;
	}
	p = strchr(tmp, ',');
	rtime_str = tmp;

	if (p != NULL) {
		*p = '\0';
		p++;
		reason_str = p;
		p = strchr(p, ',');
		if (p != NULL) {
			*p = '\0';
			arg_str = p + 1;
		}
	}
	if (prevtm != NULL) {
		*prevtm = ASN1_UTCTIME_new();
		if (!ASN1_UTCTIME_set_string(*prevtm, rtime_str)) {
			BIO_printf(bio_err, "invalid revocation date %s\n",
			    rtime_str);
			goto err;
		}
	}
	if (reason_str != NULL) {
		for (i = 0; i < NUM_REASONS; i++) {
			if (strcasecmp(reason_str, crl_reasons[i]) == 0) {
				reason_code = i;
				break;
			}
		}
		if (reason_code == OCSP_REVOKED_STATUS_NOSTATUS) {
			BIO_printf(bio_err, "invalid reason code %s\n",
			    reason_str);
			goto err;
		}
		if (reason_code == 7)
			reason_code = OCSP_REVOKED_STATUS_REMOVEFROMCRL;
		else if (reason_code == 8) {	/* Hold instruction */
			if (arg_str == NULL) {
				BIO_printf(bio_err,
				    "missing hold instruction\n");
				goto err;
			}
			reason_code = OCSP_REVOKED_STATUS_CERTIFICATEHOLD;
			hold = OBJ_txt2obj(arg_str, 0);

			if (hold == NULL) {
				BIO_printf(bio_err,
				    "invalid object identifier %s\n", arg_str);
				goto err;
			}
			if (phold != NULL)
				*phold = hold;
		} else if ((reason_code == 9) || (reason_code == 10)) {
			if (arg_str == NULL) {
				BIO_printf(bio_err,
				    "missing compromised time\n");
				goto err;
			}
			comp_time = ASN1_GENERALIZEDTIME_new();
			if (!ASN1_GENERALIZEDTIME_set_string(comp_time,
			    arg_str)) {
				BIO_printf(bio_err,
				    "invalid compromised time %s\n", arg_str);
				goto err;
			}
			if (reason_code == 9)
				reason_code = OCSP_REVOKED_STATUS_KEYCOMPROMISE;
			else
				reason_code = OCSP_REVOKED_STATUS_CACOMPROMISE;
		}
	}
	if (preason != NULL)
		*preason = reason_code;
	if (pinvtm != NULL)
		*pinvtm = comp_time;
	else
		ASN1_GENERALIZEDTIME_free(comp_time);

	ret = 1;

 err:
	free(tmp);

	if (phold == NULL)
		ASN1_OBJECT_free(hold);
	if (pinvtm == NULL)
		ASN1_GENERALIZEDTIME_free(comp_time);

	return ret;
}

static char *
bin2hex(unsigned char *data, size_t len)
{
	char *ret = NULL;
	char hex[] = "0123456789ABCDEF";
	int i;

	if ((ret = malloc(len * 2 + 1)) != NULL) {
		for (i = 0; i < len; i++) {
			ret[i * 2 + 0] = hex[data[i] >> 4];
			ret[i * 2 + 1] = hex[data[i] & 0x0F];
		}
		ret[len * 2] = '\0';
	}
	return ret;
}
