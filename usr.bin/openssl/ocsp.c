/* $OpenBSD: ocsp.c,v 1.23 2023/03/06 14:32:06 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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
#ifndef OPENSSL_NO_OCSP

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <poll.h>
#include <time.h>

/* Needs to be included before the openssl headers! */
#include "apps.h"

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

/* Maximum leeway in validity period: default 5 minutes */
#define MAX_VALIDITY_PERIOD	(5 * 60)

static int add_ocsp_cert(OCSP_REQUEST **req, X509 *cert,
    const EVP_MD *cert_id_md, X509 *issuer, STACK_OF(OCSP_CERTID) *ids);
static int add_ocsp_serial(OCSP_REQUEST **req, char *serial,
    const EVP_MD *cert_id_md, X509 *issuer, STACK_OF(OCSP_CERTID) *ids);
static int print_ocsp_summary(BIO *out, OCSP_BASICRESP *bs, OCSP_REQUEST *req,
    STACK_OF(OPENSSL_STRING) *names, STACK_OF(OCSP_CERTID) *ids, long nsec,
    long maxage);

static int make_ocsp_response(OCSP_RESPONSE **resp, OCSP_REQUEST *req,
    CA_DB *db, X509 *ca, X509 *rcert, EVP_PKEY *rkey, STACK_OF(X509) *rother,
    unsigned long flags, int nmin, int ndays);

static char **lookup_serial(CA_DB *db, ASN1_INTEGER *ser);
static BIO *init_responder(char *port);
static int do_responder(OCSP_REQUEST **preq, BIO **pcbio, BIO *acbio,
    char *port);
static int send_ocsp_response(BIO *cbio, OCSP_RESPONSE *resp);
static OCSP_RESPONSE *query_responder(BIO *err, BIO *cbio, char *path,
    STACK_OF(CONF_VALUE) *headers, const char *host, OCSP_REQUEST *req,
    int req_timeout);

static struct {
	int accept_count;
	int add_nonce;
	char *CAfile;
	char *CApath;
	X509 *cert;
	const EVP_MD *cert_id_md;
	STACK_OF(CONF_VALUE) *headers;
	char *host;
	STACK_OF(OCSP_CERTID) *ids;
	int ignore_err;
	X509 *issuer;
	char *keyfile;
	long maxage;
	int ndays;
	int nmin;
	int no_usage;
	int noverify;
	long nsec;
	char *outfile;
	char *path;
	char *port;
	char *rca_filename;
	char *rcertfile;
	OCSP_REQUEST *req;
	int req_text;
	int req_timeout;
	char *reqin;
	STACK_OF(OPENSSL_STRING) *reqnames;
	char *reqout;
	int resp_text;
	char *respin;
	char *respout;
	unsigned long rflags;
	char *ridx_filename;
	char *rkeyfile;
	char *rsignfile;
	char *sign_certfile;
	unsigned long sign_flags;
	char *signfile;
	int use_ssl;
	char *verify_certfile;
	unsigned long verify_flags;
} cfg;

static int
ocsp_opt_cert(char *arg)
{
	X509_free(cfg.cert);
	cfg.cert = load_cert(bio_err, arg, FORMAT_PEM, NULL,
	    "certificate");
	if (cfg.cert == NULL) {
		cfg.no_usage = 1;
		return (1);
	}
	if (cfg.cert_id_md == NULL)
		cfg.cert_id_md = EVP_sha1();
	if (!add_ocsp_cert(&cfg.req, cfg.cert,
	    cfg.cert_id_md, cfg.issuer, cfg.ids)) {
		cfg.no_usage = 1;
		return (1);
	}
	if (!sk_OPENSSL_STRING_push(cfg.reqnames, arg)) {
		cfg.no_usage = 1;
		return (1);
	}
	return (0);
}
	
static int
ocsp_opt_cert_id_md(int argc, char **argv, int *argsused)
{
	char *name = argv[0];

	if (*name++ != '-')
		return (1);

	if ((cfg.cert_id_md = EVP_get_digestbyname(name)) == NULL)
		return (1);

	*argsused = 1;
	return (0);
}

static int
ocsp_opt_header(int argc, char **argv, int *argsused)
{
	if (argc < 3 || argv[1] == NULL || argv[2] == NULL)
		return (1);

	if (!X509V3_add_value(argv[1], argv[2], &cfg.headers)) {
		cfg.no_usage = 1;
		return (1);
	}

	*argsused = 3;
	return (0);
}

static int
ocsp_opt_host(char *arg)
{
	if (cfg.use_ssl != -1)
		return (1);

	cfg.host = arg;
	return (0);
}

static int
ocsp_opt_issuer(char *arg)
{
	X509_free(cfg.issuer);
	cfg.issuer = load_cert(bio_err, arg, FORMAT_PEM, NULL,
	    "issuer certificate");
	if (cfg.issuer == NULL) {
		cfg.no_usage = 1;
		return (1);
	}
	return (0);
}

static int
ocsp_opt_ndays(char *arg)
{
	const char *errstr = NULL;

	cfg.ndays = strtonum(arg, 0, INT_MAX, &errstr);
	if (errstr != NULL) {
		BIO_printf(bio_err, "Illegal update period %s: %s\n",
		    arg, errstr);
		return (1);
	}
	return (0);
}

static int
ocsp_opt_nmin(char *arg)
{
	const char *errstr = NULL;

	cfg.nmin = strtonum(arg, 0, INT_MAX, &errstr);
	if (errstr != NULL) {
		BIO_printf(bio_err, "Illegal update period %s: %s\n",
		    arg, errstr);
		return (1);
	}

	if (cfg.ndays != -1)
		return (1);

	cfg.ndays = 0;
	return (0);
}

static int
ocsp_opt_nrequest(char *arg)
{
	const char *errstr = NULL;

	cfg.accept_count = strtonum(arg, 0, INT_MAX, &errstr);
	if (errstr != NULL) {
		BIO_printf(bio_err, "Illegal accept count %s: %s\n",
		    arg, errstr);
		return (1);
	}
	return (0);
}

static int
ocsp_opt_port(char *arg)
{
	if (cfg.use_ssl != -1)
		return (1);

	cfg.port = arg;
	return (0);
}

static int
ocsp_opt_serial(char *arg)
{
	if (cfg.cert_id_md == NULL)
		cfg.cert_id_md = EVP_sha1();
	if (!add_ocsp_serial(&cfg.req, arg, cfg.cert_id_md,
	    cfg.issuer, cfg.ids)) {
		cfg.no_usage = 1;
		return (1);
	}
	if (!sk_OPENSSL_STRING_push(cfg.reqnames, arg)) {
		cfg.no_usage = 1;
		return (1);
	}
	return (0);
}

static int
ocsp_opt_status_age(char *arg)
{
	const char *errstr = NULL;

	cfg.maxage = strtonum(arg, 0, LONG_MAX, &errstr);
	if (errstr != NULL) {
		BIO_printf(bio_err, "Illegal validity age %s: %s\n",
		    arg, errstr);
		return (1);
	}
	return (0);
}

static int
ocsp_opt_text(void)
{
	cfg.req_text = 1;
	cfg.resp_text = 1;
	return (0);
}

static int
ocsp_opt_timeout(char *arg)
{
	const char *errstr = NULL;

	cfg.req_timeout = strtonum(arg, 0, INT_MAX, &errstr);
	if (errstr != NULL) {
		BIO_printf(bio_err, "Illegal timeout value %s: %s\n",
		    arg, errstr);
		return (1);
	}
	return (0);
}

static int
ocsp_opt_url(char *arg)
{
	if (cfg.host == NULL && cfg.port == NULL &&
	    cfg.path == NULL) {
		if (!OCSP_parse_url(arg, &cfg.host, &cfg.port,
		    &cfg.path, &cfg.use_ssl)) {
			BIO_printf(bio_err, "Error parsing URL\n");
			return (1);
		}
	}
	return (0);
}

static int
ocsp_opt_vafile(char *arg)
{
	cfg.verify_certfile = arg;
	cfg.verify_flags |= OCSP_TRUSTOTHER;
	return (0);
}

static int
ocsp_opt_validity_period(char *arg)
{
	const char *errstr = NULL;

	cfg.nsec = strtonum(arg, 0, LONG_MAX, &errstr);
	if (errstr != NULL) {
		BIO_printf(bio_err, "Illegal validity period %s: %s\n",
		    arg, errstr);
		return (1);
	}
	return (0);
}

static const struct option ocsp_options[] = {
	{
		.name = "CA",
		.argname = "file",
		.desc = "CA certificate corresponding to the revocation information",
		.type = OPTION_ARG,
		.opt.arg = &cfg.rca_filename,
	},
	{
		.name = "CAfile",
		.argname = "file",
		.desc = "Trusted certificates file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.CAfile,
	},
	{
		.name = "CApath",
		.argname = "directory",
		.desc = "Trusted certificates directory",
		.type = OPTION_ARG,
		.opt.arg = &cfg.CApath,
	},
	{
		.name = "cert",
		.argname = "file",
		.desc = "Certificate to check",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ocsp_opt_cert,
	},
	{
		.name = "header",
		.argname = "name value",
		.desc = "Add the header name with the value to the request",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = ocsp_opt_header,
	},
	{
		.name = "host",
		.argname = "hostname:port",
		.desc = "Send OCSP request to host on port",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ocsp_opt_host,
	},
	{
		.name = "ignore_err",
		.desc = "Ignore the invalid response",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.ignore_err,
	},
	{
		.name = "index",
		.argname = "indexfile",
		.desc = "Certificate status index file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.ridx_filename,
	},
	{
		.name = "issuer",
		.argname = "file",
		.desc = "Issuer certificate",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ocsp_opt_issuer,
	},
	{
		.name = "ndays",
		.argname = "days",
		.desc = "Number of days before next update",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ocsp_opt_ndays,
	},
	{
		.name = "nmin",
		.argname = "minutes",
		.desc = "Number of minutes before next update",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ocsp_opt_nmin,
	},
	{
		.name = "no_cert_checks",
		.desc = "Don't do additional checks on signing certificate",
		.type = OPTION_UL_VALUE_OR,
		.opt.ulvalue = &cfg.verify_flags,
		.ulvalue = OCSP_NOCHECKS,
	},
	{
		.name = "no_cert_verify",
		.desc = "Don't check signing certificate",
		.type = OPTION_UL_VALUE_OR,
		.opt.ulvalue = &cfg.verify_flags,
		.ulvalue = OCSP_NOVERIFY,
	},
	{
		.name = "no_certs",
		.desc = "Don't include any certificates in signed request",
		.type = OPTION_UL_VALUE_OR,
		.opt.ulvalue = &cfg.sign_flags,
		.ulvalue = OCSP_NOCERTS,
	},
	{
		.name = "no_chain",
		.desc = "Don't use certificates in the response",
		.type = OPTION_UL_VALUE_OR,
		.opt.ulvalue = &cfg.verify_flags,
		.ulvalue = OCSP_NOCHAIN,
	},
	{
		.name = "no_explicit",
		.desc = "Don't check the explicit trust for OCSP signing",
		.type = OPTION_UL_VALUE_OR,
		.opt.ulvalue = &cfg.verify_flags,
		.ulvalue = OCSP_NOEXPLICIT,
	},
	{
		.name = "no_intern",
		.desc = "Don't search certificates contained in response for signer",
		.type = OPTION_UL_VALUE_OR,
		.opt.ulvalue = &cfg.verify_flags,
		.ulvalue = OCSP_NOINTERN,
	},
	{
		.name = "no_nonce",
		.desc = "Don't add OCSP nonce to request",
		.type = OPTION_VALUE,
		.opt.value = &cfg.add_nonce,
		.value = 0,
	},
	{
		.name = "no_signature_verify",
		.desc = "Don't check signature on response",
		.type = OPTION_UL_VALUE_OR,
		.opt.ulvalue = &cfg.verify_flags,
		.ulvalue = OCSP_NOSIGS,
	},
	{
		.name = "nonce",
		.desc = "Add OCSP nonce to request",
		.type = OPTION_VALUE,
		.opt.value = &cfg.add_nonce,
		.value = 2,
	},
	{
		.name = "noverify",
		.desc = "Don't verify response at all",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.noverify,
	},
	{
		.name = "nrequest",
		.argname = "number",
		.desc = "Number of requests to accept (default unlimited)",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ocsp_opt_nrequest,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output filename",
		.type = OPTION_ARG,
		.opt.arg = &cfg.outfile,
	},
	{
		.name = "path",
		.argname = "path",
		.desc = "Path to use in OCSP request",
		.type = OPTION_ARG,
		.opt.arg = &cfg.path,
	},
	{
		.name = "port",
		.argname = "portnum",
		.desc = "Port to run responder on",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ocsp_opt_port,
	},
	{
		.name = "req_text",
		.desc = "Print text form of request",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.req_text,
	},
	{
		.name = "reqin",
		.argname = "file",
		.desc = "Read DER encoded OCSP request from \"file\"",
		.type = OPTION_ARG,
		.opt.arg = &cfg.reqin,
	},
	{
		.name = "reqout",
		.argname = "file",
		.desc = "Write DER encoded OCSP request to \"file\"",
		.type = OPTION_ARG,
		.opt.arg = &cfg.reqout,
	},
	{
		.name = "resp_key_id",
		.desc = "Identify response by signing certificate key ID",
		.type = OPTION_UL_VALUE_OR,
		.opt.ulvalue = &cfg.rflags,
		.ulvalue = OCSP_RESPID_KEY,
	},
	{
		.name = "resp_no_certs",
		.desc = "Don't include any certificates in response",
		.type = OPTION_UL_VALUE_OR,
		.opt.ulvalue = &cfg.rflags,
		.ulvalue = OCSP_NOCERTS,
	},
	{
		.name = "resp_text",
		.desc = "Print text form of response",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.resp_text,
	},
	{
		.name = "respin",
		.argname = "file",
		.desc = "Read DER encoded OCSP response from \"file\"",
		.type = OPTION_ARG,
		.opt.arg = &cfg.respin,
	},
	{
		.name = "respout",
		.argname = "file",
		.desc = "Write DER encoded OCSP response to \"file\"",
		.type = OPTION_ARG,
		.opt.arg = &cfg.respout,
	},
	{
		.name = "rkey",
		.argname = "file",
		.desc = "Responder key to sign responses with",
		.type = OPTION_ARG,
		.opt.arg = &cfg.rkeyfile,
	},
	{
		.name = "rother",
		.argname = "file",
		.desc = "Other certificates to include in response",
		.type = OPTION_ARG,
		.opt.arg = &cfg.rcertfile,
	},
	{
		.name = "rsigner",
		.argname = "file",
		.desc = "Responder certificate to sign responses with",
		.type = OPTION_ARG,
		.opt.arg = &cfg.rsignfile,
	},
	{
		.name = "serial",
		.argname = "num",
		.desc = "Serial number to check",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ocsp_opt_serial,
	},
	{
		.name = "sign_other",
		.argname = "file",
		.desc = "Additional certificates to include in signed request",
		.type = OPTION_ARG,
		.opt.arg = &cfg.sign_certfile,
	},
	{
		.name = "signer",
		.argname = "file",
		.desc = "Certificate to sign OCSP request with",
		.type = OPTION_ARG,
		.opt.arg = &cfg.signfile,
	},
	{
		.name = "signkey",
		.argname = "file",
		.desc = "Private key to sign OCSP request with",
		.type = OPTION_ARG,
		.opt.arg = &cfg.keyfile,
	},
	{
		.name = "status_age",
		.argname = "age",
		.desc = "Maximum status age in seconds",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ocsp_opt_status_age,
	},
	{
		.name = "text",
		.desc = "Print text form of request and response",
		.type = OPTION_FUNC,
		.opt.func = ocsp_opt_text,
	},
	{
		.name = "timeout",
		.argname = "seconds",
		.desc = "Connection timeout to the OCSP responder in seconds",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ocsp_opt_timeout,
	},
	{
		.name = "trust_other",
		.desc = "Don't verify additional certificates",
		.type = OPTION_UL_VALUE_OR,
		.opt.ulvalue = &cfg.verify_flags,
		.ulvalue = OCSP_TRUSTOTHER,
	},
	{
		.name = "url",
		.argname = "responder_url",
		.desc = "OCSP responder URL",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ocsp_opt_url,
	},
	{
		.name = "VAfile",
		.argname = "file",
		.desc = "Explicitly trusted responder certificates",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ocsp_opt_vafile,
	},
	{
		.name = "validity_period",
		.argname = "n",
		.desc = "Maximum validity discrepancy in seconds",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = ocsp_opt_validity_period,
	},
	{
		.name = "verify_other",
		.argname = "file",
		.desc = "Additional certificates to search for signer",
		.type = OPTION_ARG,
		.opt.arg = &cfg.verify_certfile,
	},
	{
		.name = NULL,
		.desc = "",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = ocsp_opt_cert_id_md,
	},
	{ NULL },
};

static void
ocsp_usage(void)
{
	fprintf(stderr, "usage: ocsp "
	    "[-CA file] [-CAfile file] [-CApath directory] [-cert file]\n"
	    "    [-dgst alg] [-header name value] [-host hostname:port]\n"
	    "    [-ignore_err] [-index indexfile] [-issuer file]\n"
	    "    [-ndays days] [-nmin minutes] [-no_cert_checks]\n"
	    "    [-no_cert_verify] [-no_certs] [-no_chain] [-no_explicit]\n"
	    "    [-no_intern] [-no_nonce] [-no_signature_verify] [-nonce]\n"
	    "    [-noverify] [-nrequest number] [-out file] [-path path]\n"
	    "    [-port portnum] [-req_text] [-reqin file] [-reqout file]\n"
	    "    [-resp_key_id] [-resp_no_certs] [-resp_text] [-respin file]\n"
	    "    [-respout file] [-rkey file] [-rother file] [-rsigner file]\n"
	    "    [-serial num] [-sign_other file] [-signer file]\n"
	    "    [-signkey file] [-status_age age] [-text]\n"
	    "    [-timeout seconds] [-trust_other] [-url responder_url]\n"
	    "    [-VAfile file] [-validity_period nsec] [-verify_other file]\n");
	fprintf(stderr, "\n");
	options_usage(ocsp_options);
	fprintf(stderr, "\n");
}

int
ocsp_main(int argc, char **argv)
{
	OCSP_RESPONSE *resp = NULL;
	OCSP_BASICRESP *bs = NULL;
	X509 *signer = NULL, *rsigner = NULL;
	EVP_PKEY *key = NULL, *rkey = NULL;
	BIO *acbio = NULL, *cbio = NULL;
	BIO *derbio = NULL;
	BIO *out = NULL;
	X509_STORE *store = NULL;
	STACK_OF(X509) *sign_other = NULL, *verify_other = NULL, *rother = NULL;
	int ret = 1;
	int badarg = 0;
	int i;
	X509 *rca_cert = NULL;
	CA_DB *rdb = NULL;

	if (pledge("stdio cpath wpath rpath inet dns tty", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.accept_count = -1;
	cfg.add_nonce = 1;
	if ((cfg.ids = sk_OCSP_CERTID_new_null()) == NULL)
		goto end;
	cfg.maxage = -1;
	cfg.ndays = -1;
	cfg.nsec = MAX_VALIDITY_PERIOD;
	cfg.req_timeout = -1;
	if ((cfg.reqnames = sk_OPENSSL_STRING_new_null()) == NULL)
		goto end;
	cfg.use_ssl = -1;

	if (options_parse(argc, argv, ocsp_options, NULL, NULL) != 0) {
		if (cfg.no_usage)
			goto end;
		else
			badarg = 1;
	}

	/* Have we anything to do? */
	if (!cfg.req && !cfg.reqin && !cfg.respin &&
	    !(cfg.port && cfg.ridx_filename))
		badarg = 1;

	if (badarg) {
		ocsp_usage();
		goto end;
	}
	if (cfg.outfile)
		out = BIO_new_file(cfg.outfile, "w");
	else
		out = BIO_new_fp(stdout, BIO_NOCLOSE);

	if (!out) {
		BIO_printf(bio_err, "Error opening output file\n");
		goto end;
	}
	if (!cfg.req && (cfg.add_nonce != 2))
		cfg.add_nonce = 0;

	if (!cfg.req && cfg.reqin) {
		derbio = BIO_new_file(cfg.reqin, "rb");
		if (!derbio) {
			BIO_printf(bio_err,
			    "Error Opening OCSP request file\n");
			goto end;
		}
		cfg.req = d2i_OCSP_REQUEST_bio(derbio, NULL);
		BIO_free(derbio);
		if (!cfg.req) {
			BIO_printf(bio_err, "Error reading OCSP request\n");
			goto end;
		}
	}
	if (!cfg.req && cfg.port) {
		acbio = init_responder(cfg.port);
		if (!acbio)
			goto end;
	}
	if (cfg.rsignfile && !rdb) {
		if (!cfg.rkeyfile)
			cfg.rkeyfile = cfg.rsignfile;
		rsigner = load_cert(bio_err, cfg.rsignfile, FORMAT_PEM,
		    NULL, "responder certificate");
		if (!rsigner) {
			BIO_printf(bio_err,
			    "Error loading responder certificate\n");
			goto end;
		}
		rca_cert = load_cert(bio_err, cfg.rca_filename,
		    FORMAT_PEM, NULL, "CA certificate");
		if (cfg.rcertfile) {
			rother = load_certs(bio_err, cfg.rcertfile,
			    FORMAT_PEM, NULL, "responder other certificates");
			if (!rother)
				goto end;
		}
		rkey = load_key(bio_err, cfg.rkeyfile, FORMAT_PEM, 0,
		    NULL, "responder private key");
		if (!rkey)
			goto end;
	}
	if (acbio)
		BIO_printf(bio_err, "Waiting for OCSP client connections...\n");

 redo_accept:

	if (acbio) {
		if (!do_responder(&cfg.req, &cbio, acbio,
		    cfg.port))
			goto end;
		if (!cfg.req) {
			resp = OCSP_response_create(
			    OCSP_RESPONSE_STATUS_MALFORMEDREQUEST, NULL);
			send_ocsp_response(cbio, resp);
			goto done_resp;
		}
	}
	if (!cfg.req &&
	    (cfg.signfile || cfg.reqout || cfg.host ||
	    cfg.add_nonce || cfg.ridx_filename)) {
		BIO_printf(bio_err,
		    "Need an OCSP request for this operation!\n");
		goto end;
	}
	if (cfg.req && cfg.add_nonce)
		OCSP_request_add1_nonce(cfg.req, NULL, -1);

	if (cfg.signfile) {
		if (!cfg.keyfile)
			cfg.keyfile = cfg.signfile;
		signer = load_cert(bio_err, cfg.signfile, FORMAT_PEM,
		    NULL, "signer certificate");
		if (!signer) {
			BIO_printf(bio_err,
			    "Error loading signer certificate\n");
			goto end;
		}
		if (cfg.sign_certfile) {
			sign_other = load_certs(bio_err,
			    cfg.sign_certfile, FORMAT_PEM, NULL,
			    "signer certificates");
			if (!sign_other)
				goto end;
		}
		key = load_key(bio_err, cfg.keyfile, FORMAT_PEM, 0,
		    NULL, "signer private key");
		if (!key)
			goto end;

		if (!OCSP_request_sign(cfg.req, signer, key, NULL,
		    sign_other, cfg.sign_flags)) {
			BIO_printf(bio_err, "Error signing OCSP request\n");
			goto end;
		}
	}
	if (cfg.req_text && cfg.req)
		OCSP_REQUEST_print(out, cfg.req, 0);

	if (cfg.reqout) {
		derbio = BIO_new_file(cfg.reqout, "wb");
		if (!derbio) {
			BIO_printf(bio_err, "Error opening file %s\n",
			    cfg.reqout);
			goto end;
		}
		i2d_OCSP_REQUEST_bio(derbio, cfg.req);
		BIO_free(derbio);
	}
	if (cfg.ridx_filename && (!rkey || !rsigner || !rca_cert)) {
		BIO_printf(bio_err,
		    "Need a responder certificate, key and CA for this operation!\n");
		goto end;
	}
	if (cfg.ridx_filename && !rdb) {
		rdb = load_index(cfg.ridx_filename, NULL);
		if (!rdb)
			goto end;
		if (!index_index(rdb))
			goto end;
	}
	if (rdb) {
		i = make_ocsp_response(&resp, cfg.req, rdb, rca_cert,
		    rsigner, rkey, rother, cfg.rflags,
		    cfg.nmin, cfg.ndays);
		if (cbio)
			send_ocsp_response(cbio, resp);
	} else if (cfg.host) {
		resp = process_responder(bio_err, cfg.req,
		    cfg.host,
		    cfg.path ? cfg.path : "/",
		    cfg.port, cfg.use_ssl, cfg.headers,
		    cfg.req_timeout);
		if (!resp)
			goto end;
	} else if (cfg.respin) {
		derbio = BIO_new_file(cfg.respin, "rb");
		if (!derbio) {
			BIO_printf(bio_err,
			    "Error Opening OCSP response file\n");
			goto end;
		}
		resp = d2i_OCSP_RESPONSE_bio(derbio, NULL);
		BIO_free(derbio);
		if (!resp) {
			BIO_printf(bio_err, "Error reading OCSP response\n");
			goto end;
		}
	} else {
		ret = 0;
		goto end;
	}

 done_resp:

	if (cfg.respout) {
		derbio = BIO_new_file(cfg.respout, "wb");
		if (!derbio) {
			BIO_printf(bio_err, "Error opening file %s\n",
			    cfg.respout);
			goto end;
		}
		i2d_OCSP_RESPONSE_bio(derbio, resp);
		BIO_free(derbio);
	}
	i = OCSP_response_status(resp);

	if (i != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
		BIO_printf(bio_err, "Responder Error: %s (%d)\n",
		    OCSP_response_status_str(i), i);
		if (cfg.ignore_err)
			goto redo_accept;
		ret = 1;
		goto end;
	}
	if (cfg.resp_text)
		OCSP_RESPONSE_print(out, resp, 0);

	/* If running as responder don't verify our own response */
	if (cbio) {
		if (cfg.accept_count > 0)
			cfg.accept_count--;
		/* Redo if more connections needed */
		if (cfg.accept_count) {
			BIO_free_all(cbio);
			cbio = NULL;
			OCSP_REQUEST_free(cfg.req);
			cfg.req = NULL;
			OCSP_RESPONSE_free(resp);
			resp = NULL;
			goto redo_accept;
		}
		goto end;
	}
	if (!store)
		store = setup_verify(bio_err, cfg.CAfile,
		    cfg.CApath);
	if (!store)
		goto end;
	if (cfg.verify_certfile) {
		verify_other = load_certs(bio_err, cfg.verify_certfile,
		    FORMAT_PEM, NULL, "validator certificate");
		if (!verify_other)
			goto end;
	}
	bs = OCSP_response_get1_basic(resp);

	if (!bs) {
		BIO_printf(bio_err, "Error parsing response\n");
		goto end;
	}
	if (!cfg.noverify) {
		if (cfg.req &&
		    ((i = OCSP_check_nonce(cfg.req, bs)) <= 0)) {
			if (i == -1) {
				BIO_printf(bio_err,
				    "WARNING: no nonce in response\n");
			} else {
				BIO_printf(bio_err, "Nonce Verify error\n");
				goto end;
			}
		}
		i = OCSP_basic_verify(bs, verify_other, store,
		    cfg.verify_flags);
		if (i < 0)
			i = OCSP_basic_verify(bs, NULL, store, 0);

		if (i <= 0) {
			BIO_printf(bio_err, "Response Verify Failure\n");
			ERR_print_errors(bio_err);
		} else {
			BIO_printf(bio_err, "Response verify OK\n");
		}
	}
	if (!print_ocsp_summary(out, bs, cfg.req, cfg.reqnames,
	    cfg.ids, cfg.nsec, cfg.maxage))
		goto end;

	ret = 0;

 end:
	ERR_print_errors(bio_err);
	X509_free(signer);
	X509_STORE_free(store);
	EVP_PKEY_free(key);
	EVP_PKEY_free(rkey);
	X509_free(cfg.issuer);
	X509_free(cfg.cert);
	X509_free(rsigner);
	X509_free(rca_cert);
	free_index(rdb);
	BIO_free_all(cbio);
	BIO_free_all(acbio);
	BIO_free(out);
	OCSP_REQUEST_free(cfg.req);
	OCSP_RESPONSE_free(resp);
	OCSP_BASICRESP_free(bs);
	sk_OPENSSL_STRING_free(cfg.reqnames);
	sk_OCSP_CERTID_free(cfg.ids);
	sk_X509_pop_free(sign_other, X509_free);
	sk_X509_pop_free(verify_other, X509_free);
	sk_CONF_VALUE_pop_free(cfg.headers, X509V3_conf_free);

	if (cfg.use_ssl != -1) {
		free(cfg.host);
		free(cfg.port);
		free(cfg.path);
	}
	return (ret);
}

static int
add_ocsp_cert(OCSP_REQUEST **req, X509 *cert, const EVP_MD *cert_id_md,
    X509 *issuer, STACK_OF(OCSP_CERTID) *ids)
{
	OCSP_CERTID *id;

	if (!issuer) {
		BIO_printf(bio_err, "No issuer certificate specified\n");
		return 0;
	}
	if (!*req)
		*req = OCSP_REQUEST_new();
	if (!*req)
		goto err;
	id = OCSP_cert_to_id(cert_id_md, cert, issuer);
	if (!id || !sk_OCSP_CERTID_push(ids, id))
		goto err;
	if (!OCSP_request_add0_id(*req, id))
		goto err;
	return 1;

 err:
	BIO_printf(bio_err, "Error Creating OCSP request\n");
	return 0;
}

static int
add_ocsp_serial(OCSP_REQUEST **req, char *serial, const EVP_MD *cert_id_md,
    X509 *issuer, STACK_OF(OCSP_CERTID) *ids)
{
	OCSP_CERTID *id;
	X509_NAME *iname;
	ASN1_BIT_STRING *ikey;
	ASN1_INTEGER *sno;

	if (!issuer) {
		BIO_printf(bio_err, "No issuer certificate specified\n");
		return 0;
	}
	if (!*req)
		*req = OCSP_REQUEST_new();
	if (!*req)
		goto err;
	iname = X509_get_subject_name(issuer);
	ikey = X509_get0_pubkey_bitstr(issuer);
	sno = s2i_ASN1_INTEGER(NULL, serial);
	if (!sno) {
		BIO_printf(bio_err, "Error converting serial number %s\n",
		    serial);
		return 0;
	}
	id = OCSP_cert_id_new(cert_id_md, iname, ikey, sno);
	ASN1_INTEGER_free(sno);
	if (!id || !sk_OCSP_CERTID_push(ids, id))
		goto err;
	if (!OCSP_request_add0_id(*req, id))
		goto err;
	return 1;

 err:
	BIO_printf(bio_err, "Error Creating OCSP request\n");
	return 0;
}

static int
print_ocsp_summary(BIO *out, OCSP_BASICRESP *bs, OCSP_REQUEST *req,
    STACK_OF(OPENSSL_STRING) *names, STACK_OF(OCSP_CERTID) *ids, long nsec,
    long maxage)
{
	OCSP_CERTID *id;
	char *name;
	int i;
	int status, reason;

	ASN1_GENERALIZEDTIME *rev, *thisupd, *nextupd;

	if (!bs || !req || !sk_OPENSSL_STRING_num(names) ||
	    !sk_OCSP_CERTID_num(ids))
		return 1;

	for (i = 0; i < sk_OCSP_CERTID_num(ids); i++) {
		id = sk_OCSP_CERTID_value(ids, i);
		name = sk_OPENSSL_STRING_value(names, i);
		BIO_printf(out, "%s: ", name);

		if (!OCSP_resp_find_status(bs, id, &status, &reason,
			&rev, &thisupd, &nextupd)) {
			BIO_puts(out, "ERROR: No Status found.\n");
			continue;
		}
		/*
		 * Check validity: if invalid write to output BIO so we know
		 * which response this refers to.
		 */
		if (!OCSP_check_validity(thisupd, nextupd, nsec, maxage)) {
			BIO_puts(out, "WARNING: Status times invalid.\n");
			ERR_print_errors(out);
		}
		BIO_printf(out, "%s\n", OCSP_cert_status_str(status));

		BIO_puts(out, "\tThis Update: ");
		ASN1_GENERALIZEDTIME_print(out, thisupd);
		BIO_puts(out, "\n");

		if (nextupd) {
			BIO_puts(out, "\tNext Update: ");
			ASN1_GENERALIZEDTIME_print(out, nextupd);
			BIO_puts(out, "\n");
		}
		if (status != V_OCSP_CERTSTATUS_REVOKED)
			continue;

		if (reason != -1)
			BIO_printf(out, "\tReason: %s\n",
			    OCSP_crl_reason_str(reason));

		BIO_puts(out, "\tRevocation Time: ");
		ASN1_GENERALIZEDTIME_print(out, rev);
		BIO_puts(out, "\n");
	}

	return 1;
}


static int
make_ocsp_response(OCSP_RESPONSE **resp, OCSP_REQUEST *req, CA_DB *db,
    X509 *ca, X509 *rcert, EVP_PKEY *rkey, STACK_OF(X509) *rother,
    unsigned long flags, int nmin, int ndays)
{
	ASN1_TIME *thisupd = NULL, *nextupd = NULL;
	OCSP_CERTID *cid, *ca_id = NULL;
	OCSP_BASICRESP *bs = NULL;
	int i, id_count, ret = 1;

	id_count = OCSP_request_onereq_count(req);

	if (id_count <= 0) {
		*resp = OCSP_response_create(
		    OCSP_RESPONSE_STATUS_MALFORMEDREQUEST, NULL);
		goto end;
	}
	bs = OCSP_BASICRESP_new();
	thisupd = X509_gmtime_adj(NULL, 0);
	if (ndays != -1)
		nextupd = X509_gmtime_adj(NULL, nmin * 60 + ndays * 3600 * 24);

	/* Examine each certificate id in the request */
	for (i = 0; i < id_count; i++) {
		OCSP_ONEREQ *one;
		ASN1_INTEGER *serial;
		char **inf;
		ASN1_OBJECT *cert_id_md_oid;
		const EVP_MD *cert_id_md;
		one = OCSP_request_onereq_get0(req, i);
		cid = OCSP_onereq_get0_id(one);

		OCSP_id_get0_info(NULL, &cert_id_md_oid, NULL, NULL, cid);

		cert_id_md = EVP_get_digestbyobj(cert_id_md_oid);
		if (!cert_id_md) {
			*resp = OCSP_response_create(
			    OCSP_RESPONSE_STATUS_INTERNALERROR, NULL);
			goto end;
		}
		OCSP_CERTID_free(ca_id);
		ca_id = OCSP_cert_to_id(cert_id_md, NULL, ca);

		/* Is this request about our CA? */
		if (OCSP_id_issuer_cmp(ca_id, cid)) {
			OCSP_basic_add1_status(bs, cid,
			    V_OCSP_CERTSTATUS_UNKNOWN, 0, NULL,
			    thisupd, nextupd);
			continue;
		}
		OCSP_id_get0_info(NULL, NULL, NULL, &serial, cid);
		inf = lookup_serial(db, serial);
		if (!inf) {
			OCSP_basic_add1_status(bs, cid,
			    V_OCSP_CERTSTATUS_UNKNOWN, 0, NULL,
			    thisupd, nextupd);
		} else if (inf[DB_type][0] == DB_TYPE_VAL) {
			OCSP_basic_add1_status(bs, cid,
			    V_OCSP_CERTSTATUS_GOOD, 0, NULL,
			    thisupd, nextupd);
		} else if (inf[DB_type][0] == DB_TYPE_REV) {
			ASN1_OBJECT *inst = NULL;
			ASN1_TIME *revtm = NULL;
			ASN1_GENERALIZEDTIME *invtm = NULL;
			OCSP_SINGLERESP *single;
			int reason = -1;

			unpack_revinfo(&revtm, &reason, &inst, &invtm,
			    inf[DB_rev_date]);
			single = OCSP_basic_add1_status(bs, cid,
			    V_OCSP_CERTSTATUS_REVOKED,
			    reason, revtm,
			    thisupd, nextupd);
			if (invtm)
				OCSP_SINGLERESP_add1_ext_i2d(single,
				    NID_invalidity_date, invtm, 0, 0);
			else if (inst)
				OCSP_SINGLERESP_add1_ext_i2d(single,
				    NID_hold_instruction_code, inst, 0, 0);
			ASN1_OBJECT_free(inst);
			ASN1_TIME_free(revtm);
			ASN1_GENERALIZEDTIME_free(invtm);
		}
	}

	OCSP_copy_nonce(bs, req);

	OCSP_basic_sign(bs, rcert, rkey, NULL, rother, flags);

	*resp = OCSP_response_create(OCSP_RESPONSE_STATUS_SUCCESSFUL, bs);

 end:
	ASN1_TIME_free(thisupd);
	ASN1_TIME_free(nextupd);
	OCSP_CERTID_free(ca_id);
	OCSP_BASICRESP_free(bs);
	return ret;
}

static char **
lookup_serial(CA_DB *db, ASN1_INTEGER *ser)
{
	int i;
	BIGNUM *bn = NULL;
	char *itmp, *row[DB_NUMBER], **rrow;

	for (i = 0; i < DB_NUMBER; i++)
		row[i] = NULL;
	bn = ASN1_INTEGER_to_BN(ser, NULL);
	OPENSSL_assert(bn);	/* FIXME: should report an error at this
				 * point and abort */
	if (BN_is_zero(bn))
		itmp = strdup("00");
	else
		itmp = BN_bn2hex(bn);
	row[DB_serial] = itmp;
	BN_free(bn);
	rrow = TXT_DB_get_by_index(db->db, DB_serial, row);
	free(itmp);
	return rrow;
}

/* Quick and dirty OCSP server: read in and parse input request */

static BIO *
init_responder(char *port)
{
	BIO *acbio = NULL, *bufbio = NULL;

	bufbio = BIO_new(BIO_f_buffer());
	if (!bufbio)
		goto err;
	acbio = BIO_new_accept(port);
	if (!acbio)
		goto err;
	BIO_set_bind_mode(acbio, BIO_BIND_REUSEADDR);
	BIO_set_accept_bios(acbio, bufbio);
	bufbio = NULL;

	if (BIO_do_accept(acbio) <= 0) {
		BIO_printf(bio_err, "Error setting up accept BIO\n");
		ERR_print_errors(bio_err);
		goto err;
	}
	return acbio;

 err:
	BIO_free_all(acbio);
	BIO_free(bufbio);
	return NULL;
}

static int
do_responder(OCSP_REQUEST **preq, BIO **pcbio, BIO *acbio, char *port)
{
	int have_post = 0, len;
	OCSP_REQUEST *req = NULL;
	char inbuf[1024];
	BIO *cbio = NULL;

	if (BIO_do_accept(acbio) <= 0) {
		BIO_printf(bio_err, "Error accepting connection\n");
		ERR_print_errors(bio_err);
		return 0;
	}
	cbio = BIO_pop(acbio);
	*pcbio = cbio;

	for (;;) {
		len = BIO_gets(cbio, inbuf, sizeof inbuf);
		if (len <= 0)
			return 1;
		/* Look for "POST" signalling start of query */
		if (!have_post) {
			if (strncmp(inbuf, "POST", 4)) {
				BIO_printf(bio_err, "Invalid request\n");
				return 1;
			}
			have_post = 1;
		}
		/* Look for end of headers */
		if ((inbuf[0] == '\r') || (inbuf[0] == '\n'))
			break;
	}

	/* Try to read OCSP request */

	req = d2i_OCSP_REQUEST_bio(cbio, NULL);

	if (!req) {
		BIO_printf(bio_err, "Error parsing OCSP request\n");
		ERR_print_errors(bio_err);
	}
	*preq = req;

	return 1;
}

static int
send_ocsp_response(BIO *cbio, OCSP_RESPONSE *resp)
{
	static const char http_resp[] =
	"HTTP/1.0 200 OK\r\nContent-type: application/ocsp-response\r\n"
	"Content-Length: %d\r\n\r\n";

	if (!cbio)
		return 0;
	BIO_printf(cbio, http_resp, i2d_OCSP_RESPONSE(resp, NULL));
	i2d_OCSP_RESPONSE_bio(cbio, resp);
	(void) BIO_flush(cbio);
	return 1;
}

static OCSP_RESPONSE *
query_responder(BIO *err, BIO *cbio, char *path, STACK_OF(CONF_VALUE) *headers,
    const char *host, OCSP_REQUEST *req, int req_timeout)
{
	int fd;
	int rv;
	int i;
	int have_host = 0;
	OCSP_REQ_CTX *ctx = NULL;
	OCSP_RESPONSE *rsp = NULL;
	struct pollfd pfd[1];

	if (req_timeout != -1)
		BIO_set_nbio(cbio, 1);

	rv = BIO_do_connect(cbio);

	if ((rv <= 0) && ((req_timeout == -1) || !BIO_should_retry(cbio))) {
		BIO_puts(err, "Error connecting BIO\n");
		return NULL;
	}
	if (BIO_get_fd(cbio, &fd) < 0) {
		BIO_puts(err, "Can't get connection fd\n");
		goto err;
	}
	if (req_timeout != -1 && rv <= 0) {
		pfd[0].fd = fd;
		pfd[0].events = POLLOUT;
		rv = poll(pfd, 1, req_timeout * 1000);
		if (rv == 0) {
			BIO_puts(err, "Timeout on connect\n");
			return NULL;
		}
		if (rv == -1) {
			BIO_puts(err, "Poll error\n");
			return NULL;
		}
	}
	ctx = OCSP_sendreq_new(cbio, path, NULL, -1);
	if (!ctx)
		return NULL;

	for (i = 0; i < sk_CONF_VALUE_num(headers); i++) {
		CONF_VALUE *hdr = sk_CONF_VALUE_value(headers, i);
		if (strcasecmp("host", hdr->name) == 0)
			have_host = 1;
		if (!OCSP_REQ_CTX_add1_header(ctx, hdr->name, hdr->value))
			goto err;
	}

	if (!have_host) {
		if (!OCSP_REQ_CTX_add1_header(ctx, "Host", host))
			goto err;
	}

	if (!OCSP_REQ_CTX_set1_req(ctx, req))
		goto err;

	for (;;) {
		rv = OCSP_sendreq_nbio(&rsp, ctx);
		if (rv != -1)
			break;
		if (req_timeout == -1)
			continue;
		pfd[0].fd = fd;
		if (BIO_should_read(cbio)) {
			pfd[0].events = POLLIN;
		} else if (BIO_should_write(cbio)) {
			pfd[0].events = POLLOUT;
		} else {
			BIO_puts(err, "Unexpected retry condition\n");
			goto err;
		}
		rv = poll(pfd, 1, req_timeout * 1000);
		if (rv == 0) {
			BIO_puts(err, "Timeout on request\n");
			break;
		}
		if (rv == -1 || (pfd[0].revents & (POLLERR|POLLNVAL))) {
			BIO_puts(err, "Poll error\n");
			break;
		}
	}

 err:
	OCSP_REQ_CTX_free(ctx);
	return rsp;
}

OCSP_RESPONSE *
process_responder(BIO *err, OCSP_REQUEST *req, char *host, char *path,
    char *port, int use_ssl, STACK_OF(CONF_VALUE) *headers, int req_timeout)
{
	BIO *cbio = NULL;
	SSL_CTX *ctx = NULL;
	OCSP_RESPONSE *resp = NULL;

	cbio = BIO_new_connect(host);
	if (!cbio) {
		BIO_printf(err, "Error creating connect BIO\n");
		goto end;
	}
	if (port)
		BIO_set_conn_port(cbio, port);
	if (use_ssl == 1) {
		BIO *sbio;
		ctx = SSL_CTX_new(TLS_client_method());
		if (ctx == NULL) {
			BIO_printf(err, "Error creating SSL context.\n");
			goto end;
		}
		SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
		sbio = BIO_new_ssl(ctx, 1);
		cbio = BIO_push(sbio, cbio);
	}
	resp = query_responder(err, cbio, path, headers, host, req, req_timeout);
	if (!resp)
		BIO_printf(bio_err, "Error querying OCSP responder\n");

 end:
	BIO_free_all(cbio);
	SSL_CTX_free(ctx);
	return resp;
}
#endif
