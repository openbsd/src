/* $OpenBSD: ts.c,v 1.26 2023/03/06 14:32:06 tb Exp $ */
/* Written by Zoltan Glozik (zglozik@stones.com) for the OpenSSL
 * project 2002.
 */
/* ====================================================================
 * Copyright (c) 2001 The OpenSSL Project.  All rights reserved.
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
#include <stdlib.h>
#include <string.h>

#include "apps.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ts.h>

/* Length of the nonce of the request in bits (must be a multiple of 8). */
#define	NONCE_LENGTH		64

/* Macro definitions for the configuration file. */
#define	ENV_OID_FILE		"oid_file"

/* Local function declarations. */

static ASN1_OBJECT *txt2obj(const char *oid);
static CONF *load_config_file(const char *configfile);

/* Query related functions. */
static int query_command(const char *data, char *digest, const EVP_MD *md,
    const char *policy, int no_nonce, int cert, const char *in, const char *out,
    int text);
static BIO *BIO_open_with_default(const char *file, const char *mode,
    FILE *default_fp);
static TS_REQ *create_query(BIO *data_bio, char *digest, const EVP_MD *md,
    const char *policy, int no_nonce, int cert);
static int create_digest(BIO *input, char *digest, const EVP_MD *md,
    unsigned char **md_value);
static ASN1_INTEGER *create_nonce(int bits);

/* Reply related functions. */
static int reply_command(CONF *conf, char *section, char *queryfile,
    char *passin, char *inkey, char *signer, char *chain, const char *policy,
    char *in, int token_in, char *out, int token_out, int text);
static TS_RESP *read_PKCS7(BIO *in_bio);
static TS_RESP *create_response(CONF *conf, const char *section,
    char *queryfile, char *passin, char *inkey, char *signer, char *chain,
    const char *policy);
static ASN1_INTEGER *serial_cb(TS_RESP_CTX *ctx, void *data);
static ASN1_INTEGER *next_serial(const char *serialfile);
static int save_ts_serial(const char *serialfile, ASN1_INTEGER *serial);

/* Verify related functions. */
static int verify_command(char *data, char *digest, char *queryfile, char *in,
    int token_in, char *ca_path, char *ca_file, char *untrusted);
static TS_VERIFY_CTX *create_verify_ctx(char *data, char *digest,
    char *queryfile, char *ca_path, char *ca_file, char *untrusted);
static X509_STORE *create_cert_store(char *ca_path, char *ca_file);
static int verify_cb(int ok, X509_STORE_CTX *ctx);

enum mode {
	CMD_NONE, CMD_QUERY, CMD_REPLY, CMD_VERIFY
};

static struct {
	char *ca_file;
	char *ca_path;
	int cert;
	char *chain;
	char *configfile;
	char *data;
	char *digest;
	char *in;
	char *inkey;
	const EVP_MD *md;
	int mode;
	int no_nonce;
	char *out;
	char *passin;
	char *policy;
	char *queryfile;
	char *section;
	char *signer;
	int text;
	int token_in;
	int token_out;
	char *untrusted;
} cfg;

static int
ts_opt_md(int argc, char **argv, int *argsused)
{
	char *name = argv[0];

	if (*name++ != '-')
		return (1);

	if ((cfg.md = EVP_get_digestbyname(name)) == NULL)
		return (1);

	*argsused = 1;
	return (0);
}

static int
ts_opt_query(void)
{
	if (cfg.mode != CMD_NONE)
		return (1);
	cfg.mode = CMD_QUERY;
	return (0);
}

static int
ts_opt_reply(void)
{
	if (cfg.mode != CMD_NONE)
		return (1);
	cfg.mode = CMD_REPLY;
	return (0);
}

static int
ts_opt_verify(void)
{
	if (cfg.mode != CMD_NONE)
		return (1);
	cfg.mode = CMD_VERIFY;
	return (0);
}

static const struct option ts_options[] = {
	{
		.name = "CAfile",
		.argname = "file",
		.desc = "Certificate Authority file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.ca_file,
	},
	{
		.name = "CApath",
		.argname = "path",
		.desc = "Certificate Authority path",
		.type = OPTION_ARG,
		.opt.arg = &cfg.ca_path,
	},
	{
		.name = "cert",
		.desc = "Include signing certificate in the response",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.cert,
	},
	{
		.name = "chain",
		.argname = "file",
		.desc = "PEM certificates that will be included in the response",
		.type = OPTION_ARG,
		.opt.arg = &cfg.chain,
	},
	{
		.name = "config",
		.argname = "file",
		.desc = "Specify an alternative configuration file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.configfile,
	},
	{
		.name = "data",
		.argname = "file",
		.desc = "Data file for which the time stamp request needs to be created",
		.type = OPTION_ARG,
		.opt.arg = &cfg.data,
	},
	{
		.name = "digest",
		.argname = "arg",
		.desc = "Specify the message imprint explicitly without the data file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.digest,
	},
	{
		.name = "in",
		.argname = "file",
		.desc = "Input file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.in,
	},
	{
		.name = "inkey",
		.argname = "file",
		.desc = "Input key file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.inkey,
	},
	{
		.name = "no_nonce",
		.desc = "Specify no nonce in the request",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.no_nonce,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.out,
	},
	{
		.name = "passin",
		.argname = "src",
		.desc = "Private key password source",
		.type = OPTION_ARG,
		.opt.arg = &cfg.passin,
	},
	{
		.name = "policy",
		.argname = "object_id",
		.desc = "Policy for the TSA to use when creating the time stamp token",
		.type = OPTION_ARG,
		.opt.arg = &cfg.policy,
	},
	{
		.name = "query",
		.desc = "Create and print a time stamp request",
		.type = OPTION_FUNC,
		.opt.func = ts_opt_query,
	},
	{
		.name = "queryfile",
		.argname = "file",
		.desc = "File containing a DER-encoded time stamp request",
		.type = OPTION_ARG,
		.opt.arg = &cfg.queryfile,
	},
	{
		.name = "reply",
		.desc = "Create a time stamp response",
		.type = OPTION_FUNC,
		.opt.func = ts_opt_reply,
	},
	{
		.name = "section",
		.argname = "arg",
		.desc = "TSA section containing the settings for response generation",
		.type = OPTION_ARG,
		.opt.arg = &cfg.section,
	},
	{
		.name = "signer",
		.argname = "file",
		.desc = "Signer certificate file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.signer,
	},
	{
		.name = "text",
		.desc = "Output in human-readable text format",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.text,
	},
	{
		.name = "token_in",
		.desc = "Input is a DER-encoded time stamp token",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.token_in,
	},
	{
		.name = "token_out",
		.desc = "Output is a DER-encoded time stamp token",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.token_out,
	},
	{
		.name = "untrusted",
		.argname = "file",
		.desc = "File containing untrusted certificates",
		.type = OPTION_ARG,
		.opt.arg = &cfg.untrusted,
	},
	{
		.name = "verify",
		.desc = "Verify a time stamp response",
		.type = OPTION_FUNC,
		.opt.func = ts_opt_verify,
	},
	{
		.name = NULL,
		.desc = "",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = ts_opt_md,
	},
	{ NULL },
};

static void
ts_usage(void)
{
	fprintf(stderr, "usage:\n"
	    "ts -query [-md4 | -md5 | -ripemd160 | -sha1] [-cert]\n"
	    "    [-config configfile] [-data file_to_hash]\n"
	    "    [-digest digest_bytes] [-in request.tsq] [-no_nonce]\n"
	    "    [-out request.tsq] [-policy object_id] [-text]\n");
	fprintf(stderr, "\n"
	    "ts -reply [-chain certs_file.pem] [-config configfile]\n"
	    "    [-in response.tsr] [-inkey private.pem] [-out response.tsr]\n"
	    "    [-passin arg] [-policy object_id] [-queryfile request.tsq]\n"
	    "    [-section tsa_section] [-signer tsa_cert.pem] [-text]\n"
	    "    [-token_in] [-token_out]\n");
	fprintf(stderr, "\n"
	    "ts -verify [-CAfile trusted_certs.pem]\n"
	    "    [-CApath trusted_cert_path] [-data file_to_hash]\n"
	    "    [-digest digest_bytes] [-in response.tsr]\n"
	    "    [-queryfile request.tsq] [-token_in]\n"
	    "    [-untrusted cert_file.pem]\n");
	fprintf(stderr, "\n");
	options_usage(ts_options);
	fprintf(stderr, "\n");
}

int
ts_main(int argc, char **argv)
{
	int ret = 1;
	CONF *conf = NULL;
	char *password = NULL;	/* Password itself. */

	if (pledge("stdio cpath wpath rpath tty", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = CMD_NONE;

	if (options_parse(argc, argv, ts_options, NULL, NULL) != 0)
		goto usage;

	/* Get the password if required. */
	if (cfg.mode == CMD_REPLY && cfg.passin != NULL &&
	    !app_passwd(bio_err, cfg.passin, NULL, &password, NULL)) {
		BIO_printf(bio_err, "Error getting password.\n");
		goto cleanup;
	}
	/*
	 * Check consistency of parameters and execute the appropriate
	 * function.
	 */
	switch (cfg.mode) {
	case CMD_NONE:
		goto usage;
	case CMD_QUERY:
		/*
		 * Data file and message imprint cannot be specified at the
		 * same time.
		 */
		ret = cfg.data != NULL && cfg.digest != NULL;
		if (ret)
			goto usage;
		/* Load the config file for possible policy OIDs. */
		conf = load_config_file(cfg.configfile);
		ret = !query_command(cfg.data, cfg.digest,
		    cfg.md, cfg.policy, cfg.no_nonce,
		    cfg.cert, cfg.in, cfg.out,
		    cfg.text);
		break;
	case CMD_REPLY:
		conf = load_config_file(cfg.configfile);
		if (cfg.in == NULL) {
			ret = !(cfg.queryfile != NULL && conf != NULL &&
			    !cfg.token_in);
			if (ret)
				goto usage;
		} else {
			/* 'in' and 'queryfile' are exclusive. */
			ret = !(cfg.queryfile == NULL);
			if (ret)
				goto usage;
		}

		ret = !reply_command(conf, cfg.section,
		    cfg.queryfile, password, cfg.inkey,
		    cfg.signer, cfg.chain, cfg.policy,
		    cfg.in, cfg.token_in, cfg.out,
		    cfg.token_out, cfg.text);
		break;
	case CMD_VERIFY:
		ret = !(((cfg.queryfile != NULL && cfg.data == NULL &&
		    cfg.digest == NULL) ||
		    (cfg.queryfile == NULL && cfg.data != NULL &&
		    cfg.digest == NULL) ||
		    (cfg.queryfile == NULL && cfg.data == NULL &&
		    cfg.digest != NULL)) &&
		    cfg.in != NULL);
		if (ret)
			goto usage;

		ret = !verify_command(cfg.data, cfg.digest,
		    cfg.queryfile, cfg.in, cfg.token_in,
		    cfg.ca_path, cfg.ca_file, cfg.untrusted);
	}

	goto cleanup;

 usage:
	ts_usage();

 cleanup:
	/* Clean up. */
	NCONF_free(conf);
	free(password);
	OBJ_cleanup();

	return (ret);
}

/*
 * Configuration file-related function definitions.
 */

static ASN1_OBJECT *
txt2obj(const char *oid)
{
	ASN1_OBJECT *oid_obj = NULL;

	if ((oid_obj = OBJ_txt2obj(oid, 0)) == NULL)
		BIO_printf(bio_err, "cannot convert %s to OID\n", oid);

	return oid_obj;
}

static CONF *
load_config_file(const char *configfile)
{
	CONF *conf = NULL;
	long errorline = -1;

	if (configfile == NULL)
		configfile = getenv("OPENSSL_CONF");

	if (configfile != NULL &&
	    ((conf = NCONF_new(NULL)) == NULL ||
	    NCONF_load(conf, configfile, &errorline) <= 0)) {
		if (errorline <= 0)
			BIO_printf(bio_err, "error loading the config file "
			    "'%s'\n", configfile);
		else
			BIO_printf(bio_err, "error on line %ld of config file "
			    "'%s'\n", errorline, configfile);
	}
	if (conf != NULL) {
		const char *p;

		BIO_printf(bio_err, "Using configuration from %s\n",
		    configfile);
		p = NCONF_get_string(conf, NULL, ENV_OID_FILE);
		if (p != NULL) {
			BIO *oid_bio = BIO_new_file(p, "r");
			if (oid_bio == NULL)
				ERR_print_errors(bio_err);
			else {
				OBJ_create_objects(oid_bio);
				BIO_free_all(oid_bio);
			}
		} else
			ERR_clear_error();
		if (!add_oid_section(bio_err, conf))
			ERR_print_errors(bio_err);
	}
	return conf;
}

/*
 * Query-related method definitions.
 */

static int
query_command(const char *data, char *digest, const EVP_MD *md,
    const char *policy, int no_nonce, int cert, const char *in, const char *out,
    int text)
{
	int ret = 0;
	TS_REQ *query = NULL;
	BIO *in_bio = NULL;
	BIO *data_bio = NULL;
	BIO *out_bio = NULL;

	/* Build query object either from file or from scratch. */
	if (in != NULL) {
		if ((in_bio = BIO_new_file(in, "rb")) == NULL)
			goto end;
		query = d2i_TS_REQ_bio(in_bio, NULL);
	} else {
		/* Open the file if no explicit digest bytes were specified. */
		if (digest == NULL &&
		    (data_bio = BIO_open_with_default(data, "rb", stdin)) == NULL)
			goto end;
		/* Creating the query object. */
		query = create_query(data_bio, digest, md,
		    policy, no_nonce, cert);
		/* Saving the random number generator state. */
	}
	if (query == NULL)
		goto end;

	/* Write query either in ASN.1 or in text format. */
	if ((out_bio = BIO_open_with_default(out, "wb", stdout)) == NULL)
		goto end;
	if (text) {
		/* Text output. */
		if (!TS_REQ_print_bio(out_bio, query))
			goto end;
	} else {
		/* ASN.1 output. */
		if (!i2d_TS_REQ_bio(out_bio, query))
			goto end;
	}

	ret = 1;

 end:
	ERR_print_errors(bio_err);

	/* Clean up. */
	BIO_free_all(in_bio);
	BIO_free_all(data_bio);
	BIO_free_all(out_bio);
	TS_REQ_free(query);

	return ret;
}

static BIO *
BIO_open_with_default(const char *file, const char *mode, FILE *default_fp)
{
	return file == NULL ? BIO_new_fp(default_fp, BIO_NOCLOSE) :
	    BIO_new_file(file, mode);
}

static TS_REQ *
create_query(BIO *data_bio, char *digest, const EVP_MD *md, const char *policy,
    int no_nonce, int cert)
{
	int ret = 0;
	TS_REQ *ts_req = NULL;
	int len;
	TS_MSG_IMPRINT *msg_imprint = NULL;
	X509_ALGOR *algo = NULL;
	unsigned char *data = NULL;
	ASN1_OBJECT *policy_obj = NULL;
	ASN1_INTEGER *nonce_asn1 = NULL;

	/* Setting default message digest. */
	if (md == NULL && (md = EVP_get_digestbyname("sha1")) == NULL)
		goto err;

	/* Creating request object. */
	if ((ts_req = TS_REQ_new()) == NULL)
		goto err;

	/* Setting version. */
	if (!TS_REQ_set_version(ts_req, 1))
		goto err;

	/* Creating and adding MSG_IMPRINT object. */
	if ((msg_imprint = TS_MSG_IMPRINT_new()) == NULL)
		goto err;

	/* Adding algorithm. */
	if ((algo = X509_ALGOR_new()) == NULL)
		goto err;
	if ((algo->algorithm = OBJ_nid2obj(EVP_MD_type(md))) == NULL)
		goto err;
	if ((algo->parameter = ASN1_TYPE_new()) == NULL)
		goto err;
	algo->parameter->type = V_ASN1_NULL;
	if (!TS_MSG_IMPRINT_set_algo(msg_imprint, algo))
		goto err;

	/* Adding message digest. */
	if ((len = create_digest(data_bio, digest, md, &data)) == 0)
		goto err;
	if (!TS_MSG_IMPRINT_set_msg(msg_imprint, data, len))
		goto err;

	if (!TS_REQ_set_msg_imprint(ts_req, msg_imprint))
		goto err;

	/* Setting policy if requested. */
	if (policy != NULL && (policy_obj = txt2obj(policy)) == NULL)
		goto err;
	if (policy_obj != NULL && !TS_REQ_set_policy_id(ts_req, policy_obj))
		goto err;

	/* Setting nonce if requested. */
	if (!no_nonce && (nonce_asn1 = create_nonce(NONCE_LENGTH)) == NULL)
		goto err;
	if (nonce_asn1 != NULL && !TS_REQ_set_nonce(ts_req, nonce_asn1))
		goto err;

	/* Setting certificate request flag if requested. */
	if (!TS_REQ_set_cert_req(ts_req, cert))
		goto err;

	ret = 1;

 err:
	if (!ret) {
		TS_REQ_free(ts_req);
		ts_req = NULL;
		BIO_printf(bio_err, "could not create query\n");
	}
	TS_MSG_IMPRINT_free(msg_imprint);
	X509_ALGOR_free(algo);
	free(data);
	ASN1_OBJECT_free(policy_obj);
	ASN1_INTEGER_free(nonce_asn1);

	return ts_req;
}

static int
create_digest(BIO *input, char *digest, const EVP_MD *md,
    unsigned char **md_value)
{
	int md_value_len;
	EVP_MD_CTX *md_ctx = NULL;

	md_value_len = EVP_MD_size(md);
	if (md_value_len < 0)
		goto err;

	if (input != NULL) {
		/* Digest must be computed from an input file. */
		unsigned char buffer[4096];
		int length;

		*md_value = malloc(md_value_len);
		if (*md_value == NULL)
			goto err;

		if ((md_ctx = EVP_MD_CTX_new()) == NULL)
			goto err;

		if (!EVP_DigestInit(md_ctx, md))
			goto err;

		while ((length = BIO_read(input, buffer, sizeof(buffer))) > 0) {
			if (!EVP_DigestUpdate(md_ctx, buffer, length))
				goto err;
		}

		if (!EVP_DigestFinal(md_ctx, *md_value, NULL))
			goto err;

		EVP_MD_CTX_free(md_ctx);
		md_ctx = NULL;

	} else {
		/* Digest bytes are specified with digest. */
		long digest_len;

		*md_value = string_to_hex(digest, &digest_len);
		if (*md_value == NULL || md_value_len != digest_len) {
			free(*md_value);
			*md_value = NULL;
			BIO_printf(bio_err, "bad digest, %d bytes "
			    "must be specified\n", md_value_len);
			goto err;
		}
	}

	return md_value_len;

 err:
	EVP_MD_CTX_free(md_ctx);
	return 0;
}

static ASN1_INTEGER *
create_nonce(int bits)
{
	unsigned char buf[20];
	ASN1_INTEGER *nonce = NULL;
	int len = (bits - 1) / 8 + 1;
	int i;

	/* Generating random byte sequence. */
	if (len > (int) sizeof(buf))
		goto err;
	arc4random_buf(buf, len);

	/* Find the first non-zero byte and creating ASN1_INTEGER object. */
	for (i = 0; i < len && !buf[i]; ++i)
		;
	if ((nonce = ASN1_INTEGER_new()) == NULL)
		goto err;
	free(nonce->data);
	/* Allocate at least one byte. */
	nonce->length = len - i;
	if ((nonce->data = malloc(nonce->length + 1)) == NULL)
		goto err;
	memcpy(nonce->data, buf + i, nonce->length);

	return nonce;

 err:
	BIO_printf(bio_err, "could not create nonce\n");
	ASN1_INTEGER_free(nonce);
	return NULL;
}

/*
 * Reply-related method definitions.
 */

static int
reply_command(CONF *conf, char *section, char *queryfile, char *passin,
    char *inkey, char *signer, char *chain, const char *policy, char *in,
    int token_in, char *out, int token_out, int text)
{
	int ret = 0;
	TS_RESP *response = NULL;
	BIO *in_bio = NULL;
	BIO *query_bio = NULL;
	BIO *inkey_bio = NULL;
	BIO *signer_bio = NULL;
	BIO *out_bio = NULL;

	/* Build response object either from response or query. */
	if (in != NULL) {
		if ((in_bio = BIO_new_file(in, "rb")) == NULL)
			goto end;
		if (token_in) {
			/*
			 * We have a ContentInfo (PKCS7) object, add
			 * 'granted' status info around it.
			 */
			response = read_PKCS7(in_bio);
		} else {
			/* We have a ready-made TS_RESP object. */
			response = d2i_TS_RESP_bio(in_bio, NULL);
		}
	} else {
		response = create_response(conf, section, queryfile, passin,
				inkey, signer, chain, policy);
		if (response != NULL)
			BIO_printf(bio_err, "Response has been generated.\n");
		else
			BIO_printf(bio_err, "Response is not generated.\n");
	}
	if (response == NULL)
		goto end;

	/* Write response either in ASN.1 or text format. */
	if ((out_bio = BIO_open_with_default(out, "wb", stdout)) == NULL)
		goto end;
	if (text) {
		/* Text output. */
		if (token_out) {
			TS_TST_INFO *tst_info = TS_RESP_get_tst_info(response);
			if (!TS_TST_INFO_print_bio(out_bio, tst_info))
				goto end;
		} else {
			if (!TS_RESP_print_bio(out_bio, response))
				goto end;
		}
	} else {
		/* ASN.1 DER output. */
		if (token_out) {
			PKCS7 *token = TS_RESP_get_token(response);
			if (!i2d_PKCS7_bio(out_bio, token))
				goto end;
		} else {
			if (!i2d_TS_RESP_bio(out_bio, response))
				goto end;
		}
	}

	ret = 1;

 end:
	ERR_print_errors(bio_err);

	/* Clean up. */
	BIO_free_all(in_bio);
	BIO_free_all(query_bio);
	BIO_free_all(inkey_bio);
	BIO_free_all(signer_bio);
	BIO_free_all(out_bio);
	TS_RESP_free(response);

	return ret;
}

/* Reads a PKCS7 token and adds default 'granted' status info to it. */
static TS_RESP *
read_PKCS7(BIO *in_bio)
{
	int ret = 0;
	PKCS7 *token = NULL;
	TS_TST_INFO *tst_info = NULL;
	TS_RESP *resp = NULL;
	TS_STATUS_INFO *si = NULL;

	/* Read PKCS7 object and extract the signed time stamp info. */
	if ((token = d2i_PKCS7_bio(in_bio, NULL)) == NULL)
		goto end;
	if ((tst_info = PKCS7_to_TS_TST_INFO(token)) == NULL)
		goto end;

	/* Creating response object. */
	if ((resp = TS_RESP_new()) == NULL)
		goto end;

	/* Create granted status info. */
	if ((si = TS_STATUS_INFO_new()) == NULL)
		goto end;
	if (!TS_STATUS_INFO_set_status(si, TS_STATUS_GRANTED))
		goto end;
	if (!TS_RESP_set_status_info(resp, si))
		goto end;

	/* Setting encapsulated token. */
	TS_RESP_set_tst_info(resp, token, tst_info);
	token = NULL;		/* Ownership is lost. */
	tst_info = NULL;	/* Ownership is lost. */

	ret = 1;
 end:
	PKCS7_free(token);
	TS_TST_INFO_free(tst_info);
	if (!ret) {
		TS_RESP_free(resp);
		resp = NULL;
	}
	TS_STATUS_INFO_free(si);
	return resp;
}

static TS_RESP *
create_response(CONF *conf, const char *section, char *queryfile, char *passin,
    char *inkey, char *signer, char *chain, const char *policy)
{
	int ret = 0;
	TS_RESP *response = NULL;
	BIO *query_bio = NULL;
	TS_RESP_CTX *resp_ctx = NULL;

	if ((query_bio = BIO_new_file(queryfile, "rb")) == NULL)
		goto end;

	/* Getting TSA configuration section. */
	if ((section = TS_CONF_get_tsa_section(conf, section)) == NULL)
		goto end;

	/* Setting up response generation context. */
	if ((resp_ctx = TS_RESP_CTX_new()) == NULL)
		goto end;

	/* Setting serial number provider callback. */
	if (!TS_CONF_set_serial(conf, section, serial_cb, resp_ctx))
		goto end;

	/* Setting TSA signer certificate. */
	if (!TS_CONF_set_signer_cert(conf, section, signer, resp_ctx))
		goto end;

	/* Setting TSA signer certificate chain. */
	if (!TS_CONF_set_certs(conf, section, chain, resp_ctx))
		goto end;

	/* Setting TSA signer private key. */
	if (!TS_CONF_set_signer_key(conf, section, inkey, passin, resp_ctx))
		goto end;

	/* Setting default policy OID. */
	if (!TS_CONF_set_def_policy(conf, section, policy, resp_ctx))
		goto end;

	/* Setting acceptable policy OIDs. */
	if (!TS_CONF_set_policies(conf, section, resp_ctx))
		goto end;

	/* Setting the acceptable one-way hash algorithms. */
	if (!TS_CONF_set_digests(conf, section, resp_ctx))
		goto end;

	/* Setting guaranteed time stamp accuracy. */
	if (!TS_CONF_set_accuracy(conf, section, resp_ctx))
		goto end;

	/* Setting the precision of the time. */
	if (!TS_CONF_set_clock_precision_digits(conf, section, resp_ctx))
		goto end;

	/* Setting the ordering flaf if requested. */
	if (!TS_CONF_set_ordering(conf, section, resp_ctx))
		goto end;

	/* Setting the TSA name required flag if requested. */
	if (!TS_CONF_set_tsa_name(conf, section, resp_ctx))
		goto end;

	/* Setting the ESS cert id chain flag if requested. */
	if (!TS_CONF_set_ess_cert_id_chain(conf, section, resp_ctx))
		goto end;

	/* Creating the response. */
	if ((response = TS_RESP_create_response(resp_ctx, query_bio)) == NULL)
		goto end;

	ret = 1;
 end:
	if (!ret) {
		TS_RESP_free(response);
		response = NULL;
	}
	TS_RESP_CTX_free(resp_ctx);
	BIO_free_all(query_bio);

	return response;
}

static ASN1_INTEGER *
serial_cb(TS_RESP_CTX *ctx, void *data)
{
	const char *serial_file = (const char *) data;
	ASN1_INTEGER *serial = next_serial(serial_file);

	if (serial == NULL) {
		TS_RESP_CTX_set_status_info(ctx, TS_STATUS_REJECTION,
		    "Error during serial number "
		    "generation.");
		TS_RESP_CTX_add_failure_info(ctx,
		    TS_INFO_ADD_INFO_NOT_AVAILABLE);
	} else
		save_ts_serial(serial_file, serial);

	return serial;
}

static ASN1_INTEGER *
next_serial(const char *serialfile)
{
	int ret = 0;
	BIO *in = NULL;
	ASN1_INTEGER *serial = NULL;
	BIGNUM *bn = NULL;

	if ((serial = ASN1_INTEGER_new()) == NULL)
		goto err;

	if ((in = BIO_new_file(serialfile, "r")) == NULL) {
		ERR_clear_error();
		BIO_printf(bio_err, "Warning: could not open file %s for "
		    "reading, using serial number: 1\n", serialfile);
		if (!ASN1_INTEGER_set(serial, 1))
			goto err;
	} else {
		char buf[1024];
		if (!a2i_ASN1_INTEGER(in, serial, buf, sizeof(buf))) {
			BIO_printf(bio_err, "unable to load number from %s\n",
			    serialfile);
			goto err;
		}
		if ((bn = ASN1_INTEGER_to_BN(serial, NULL)) == NULL)
			goto err;
		ASN1_INTEGER_free(serial);
		serial = NULL;
		if (!BN_add_word(bn, 1))
			goto err;
		if ((serial = BN_to_ASN1_INTEGER(bn, NULL)) == NULL)
			goto err;
	}
	ret = 1;
 err:
	if (!ret) {
		ASN1_INTEGER_free(serial);
		serial = NULL;
	}
	BIO_free_all(in);
	BN_free(bn);
	return serial;
}

static int
save_ts_serial(const char *serialfile, ASN1_INTEGER *serial)
{
	int ret = 0;
	BIO *out = NULL;

	if ((out = BIO_new_file(serialfile, "w")) == NULL)
		goto err;
	if (i2a_ASN1_INTEGER(out, serial) <= 0)
		goto err;
	if (BIO_puts(out, "\n") <= 0)
		goto err;
	ret = 1;
 err:
	if (!ret)
		BIO_printf(bio_err, "could not save serial number to %s\n",
		    serialfile);
	BIO_free_all(out);
	return ret;
}

/*
 * Verify-related method definitions.
 */

static int
verify_command(char *data, char *digest, char *queryfile, char *in,
    int token_in, char *ca_path, char *ca_file, char *untrusted)
{
	BIO *in_bio = NULL;
	PKCS7 *token = NULL;
	TS_RESP *response = NULL;
	TS_VERIFY_CTX *verify_ctx = NULL;
	int ret = 0;

	/* Decode the token (PKCS7) or response (TS_RESP) files. */
	if ((in_bio = BIO_new_file(in, "rb")) == NULL)
		goto end;
	if (token_in) {
		if ((token = d2i_PKCS7_bio(in_bio, NULL)) == NULL)
			goto end;
	} else {
		if ((response = d2i_TS_RESP_bio(in_bio, NULL)) == NULL)
			goto end;
	}

	if ((verify_ctx = create_verify_ctx(data, digest, queryfile,
	    ca_path, ca_file, untrusted)) == NULL)
		goto end;

	/* Checking the token or response against the request. */
	ret = token_in ?
	    TS_RESP_verify_token(verify_ctx, token) :
	    TS_RESP_verify_response(verify_ctx, response);

 end:
	printf("Verification: ");
	if (ret)
		printf("OK\n");
	else {
		printf("FAILED\n");
		/* Print errors, if there are any. */
		ERR_print_errors(bio_err);
	}

	/* Clean up. */
	BIO_free_all(in_bio);
	PKCS7_free(token);
	TS_RESP_free(response);
	TS_VERIFY_CTX_free(verify_ctx);
	return ret;
}

static TS_VERIFY_CTX *
create_verify_ctx(char *data, char *digest, char *queryfile, char *ca_path,
    char *ca_file, char *untrusted)
{
	TS_VERIFY_CTX *ctx = NULL;
	BIO *input = NULL;
	TS_REQ *request = NULL;
	X509_STORE *store;
	STACK_OF(X509) *certs;
	int ret = 0;

	if (data != NULL || digest != NULL) {
		if ((ctx = TS_VERIFY_CTX_new()) == NULL)
			goto err;
		TS_VERIFY_CTX_set_flags(ctx, TS_VFY_VERSION | TS_VFY_SIGNER);
		if (data != NULL) {
			BIO *data_bio;

			TS_VERIFY_CTX_add_flags(ctx, TS_VFY_DATA);
			if ((data_bio = BIO_new_file(data, "rb")) == NULL)
				goto err;
			TS_VERIFY_CTX_set_data(ctx, data_bio);
		} else if (digest != NULL) {
			unsigned char *imprint;
			long imprint_len;

			TS_VERIFY_CTX_add_flags(ctx, TS_VFY_IMPRINT);
			if ((imprint = string_to_hex(digest,
			    &imprint_len)) == NULL) {
				BIO_printf(bio_err, "invalid digest string\n");
				goto err;
			}
			TS_VERIFY_CTX_set_imprint(ctx, imprint, imprint_len);
		}
	} else if (queryfile != NULL) {
		/*
		 * The request has just to be read, decoded and converted to
		 * a verify context object.
		 */
		if ((input = BIO_new_file(queryfile, "rb")) == NULL)
			goto err;
		if ((request = d2i_TS_REQ_bio(input, NULL)) == NULL)
			goto err;
		if ((ctx = TS_REQ_to_TS_VERIFY_CTX(request, NULL)) == NULL)
			goto err;
	} else
		return NULL;

	/* Add the signature verification flag and arguments. */
	TS_VERIFY_CTX_add_flags(ctx, TS_VFY_SIGNATURE);

	/* Initialising the X509_STORE object. */
	if ((store = create_cert_store(ca_path, ca_file)) == NULL)
		goto err;
	TS_VERIFY_CTX_set_store(ctx, store);

	/* Loading untrusted certificates. */
	if (untrusted != NULL) {
		if ((certs = TS_CONF_load_certs(untrusted)) == NULL)
			goto err;
		TS_VERIFY_CTX_set_certs(ctx, certs);
	}

	ret = 1;
 err:
	if (!ret) {
		TS_VERIFY_CTX_free(ctx);
		ctx = NULL;
	}
	BIO_free_all(input);
	TS_REQ_free(request);
	return ctx;
}

static X509_STORE *
create_cert_store(char *ca_path, char *ca_file)
{
	X509_STORE *cert_ctx = NULL;
	X509_LOOKUP *lookup = NULL;
	int i;

	/* Creating the X509_STORE object. */
	if ((cert_ctx = X509_STORE_new()) == NULL)
		goto err;

	/* Setting the callback for certificate chain verification. */
	X509_STORE_set_verify_cb(cert_ctx, verify_cb);

	/* Adding a trusted certificate directory source. */
	if (ca_path != NULL) {
		lookup = X509_STORE_add_lookup(cert_ctx,
		    X509_LOOKUP_hash_dir());
		if (lookup == NULL) {
			BIO_printf(bio_err, "memory allocation failure\n");
			goto err;
		}
		i = X509_LOOKUP_add_dir(lookup, ca_path, X509_FILETYPE_PEM);
		if (!i) {
			BIO_printf(bio_err, "Error loading directory %s\n",
			    ca_path);
			goto err;
		}
	}
	/* Adding a trusted certificate file source. */
	if (ca_file != NULL) {
		lookup = X509_STORE_add_lookup(cert_ctx, X509_LOOKUP_file());
		if (lookup == NULL) {
			BIO_printf(bio_err, "memory allocation failure\n");
			goto err;
		}
		i = X509_LOOKUP_load_file(lookup, ca_file, X509_FILETYPE_PEM);
		if (!i) {
			BIO_printf(bio_err, "Error loading file %s\n", ca_file);
			goto err;
		}
	}
	return cert_ctx;
 err:
	X509_STORE_free(cert_ctx);
	return NULL;
}

static int
verify_cb(int ok, X509_STORE_CTX *ctx)
{
	/*
	char buf[256];

	if (!ok)
		{
		X509_NAME_oneline(X509_get_subject_name(ctx->current_cert),
				  buf, sizeof(buf));
		printf("%s\n", buf);
		printf("error %d at %d depth lookup: %s\n",
		       ctx->error, ctx->error_depth,
			X509_verify_cert_error_string(ctx->error));
		}
	*/

	return ok;
}
