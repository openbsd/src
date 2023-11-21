/* $OpenBSD: cms.c,v 1.35 2023/11/21 17:56:19 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project.  All rights reserved.
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
 */

/* CMS utility function */

#include <stdio.h>
#include <string.h>

#include "apps.h"

#ifndef OPENSSL_NO_CMS

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

#include <openssl/cms.h>

static int save_certs(char *signerfile, STACK_OF(X509) *signers);
static void receipt_request_print(BIO *out, CMS_ContentInfo *cms);
static CMS_ReceiptRequest *make_receipt_request(
    STACK_OF(OPENSSL_STRING) *rr_to, int rr_allorfirst,
    STACK_OF(OPENSSL_STRING) *rr_from);
static int cms_set_pkey_param(EVP_PKEY_CTX *pctx,
    STACK_OF(OPENSSL_STRING) *param);

#define SMIME_OP	0x10
#define SMIME_IP	0x20
#define SMIME_SIGNERS	0x40
#define SMIME_ENCRYPT		(1 | SMIME_OP)
#define SMIME_DECRYPT		(2 | SMIME_IP)
#define SMIME_SIGN		(3 | SMIME_OP | SMIME_SIGNERS)
#define SMIME_VERIFY		(4 | SMIME_IP)
#define SMIME_CMSOUT		(5 | SMIME_IP | SMIME_OP)
#define SMIME_RESIGN		(6 | SMIME_IP | SMIME_OP | SMIME_SIGNERS)
#define SMIME_DATAOUT		(7 | SMIME_IP)
#define SMIME_DATA_CREATE	(8 | SMIME_OP)
#define SMIME_DIGEST_VERIFY	(9 | SMIME_IP)
#define SMIME_DIGEST_CREATE	(10 | SMIME_OP)
#define SMIME_UNCOMPRESS	(11 | SMIME_IP)
#define SMIME_COMPRESS		(12 | SMIME_OP)
#define SMIME_ENCRYPTED_DECRYPT	(13 | SMIME_IP)
#define SMIME_ENCRYPTED_ENCRYPT	(14 | SMIME_OP)
#define SMIME_SIGN_RECEIPT	(15 | SMIME_IP | SMIME_OP)
#define SMIME_VERIFY_RECEIPT	(16 | SMIME_IP)

int verify_err = 0;

struct cms_key_param {
	int idx;
	STACK_OF(OPENSSL_STRING) *param;
	struct cms_key_param *next;
};

static struct {
	char *CAfile;
	char *CApath;
	X509 *cert;
	char *certfile;
	char *certsoutfile;
	const EVP_CIPHER *cipher;
	char *contfile;
	ASN1_OBJECT *econtent_type;
	STACK_OF(X509) *encerts;
	int flags;
	char *from;
	char *infile;
	int informat;
	struct cms_key_param *key_first;
	struct cms_key_param *key_param;
	char *keyfile;
	int keyform;
	int noout;
	int operation;
	char *outfile;
	int outformat;
	char *passargin;
	int print;
	unsigned char *pwri_pass;
	int rr_allorfirst;
	STACK_OF(OPENSSL_STRING) *rr_from;
	int rr_print;
	STACK_OF(OPENSSL_STRING) *rr_to;
	char *rctfile;
	int rctformat;
	char *recipfile;
	unsigned char *secret_key;
	unsigned char *secret_keyid;
	size_t secret_keyidlen;
	size_t secret_keylen;
	const EVP_MD *sign_md;
	char *signerfile;
	STACK_OF(OPENSSL_STRING) *skkeys;
	STACK_OF(OPENSSL_STRING) *sksigners;
	char *subject;
	char *to;
	int verify_retcode;
	X509_VERIFY_PARAM *vpm;
} cfg;

static const EVP_CIPHER *
get_cipher_by_name(char *name)
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
#ifndef OPENSSL_NO_RC2
	else if (!strcmp(name, "rc2-40"))
		return EVP_rc2_40_cbc();
	else if (!strcmp(name, "rc2-64"))
		return EVP_rc2_64_cbc();
	else if (!strcmp(name, "rc2-128"))
		return EVP_rc2_cbc();
#endif
	else
		return (NULL);
}

static int
cms_opt_cipher(int argc, char **argv, int *argsused)
{
	char *name = argv[0];

	if (*name++ != '-')
		return (1);

	if ((cfg.cipher = get_cipher_by_name(name)) == NULL)
		if ((cfg.cipher = EVP_get_cipherbyname(name)) == NULL)
			return (1);

	*argsused = 1;
	return (0);
}

static int
cms_opt_econtent_type(char *arg)
{
	ASN1_OBJECT_free(cfg.econtent_type);

	if ((cfg.econtent_type = OBJ_txt2obj(arg, 0)) == NULL) {
		BIO_printf(bio_err, "Invalid OID %s\n", arg);
		return (1);
	}
	return (0);
}

static int
cms_opt_inkey(char *arg)
{
	if (cfg.keyfile == NULL) {
		cfg.keyfile = arg;
		return (0);
	}

	if (cfg.signerfile == NULL) {
		BIO_puts(bio_err, "Illegal -inkey without -signer\n");
		return (1);
	}

	if (cfg.sksigners == NULL)
		cfg.sksigners = sk_OPENSSL_STRING_new_null();
	if (cfg.sksigners == NULL)
		return (1);
	if (!sk_OPENSSL_STRING_push(cfg.sksigners, cfg.signerfile))
		return (1);

	cfg.signerfile = NULL;

	if (cfg.skkeys == NULL)
		cfg.skkeys = sk_OPENSSL_STRING_new_null();
	if (cfg.skkeys == NULL)
		return (1);
	if (!sk_OPENSSL_STRING_push(cfg.skkeys, cfg.keyfile))
		return (1);

	cfg.keyfile = arg;
	return (0);
}

static int
cms_opt_keyopt(char *arg)
{
	int keyidx = -1;

	if (cfg.operation == SMIME_ENCRYPT) {
		if (cfg.encerts != NULL)
			keyidx += sk_X509_num(cfg.encerts);
	} else {
		if (cfg.keyfile != NULL || cfg.signerfile != NULL)
			keyidx++;
		if (cfg.skkeys != NULL)
			keyidx += sk_OPENSSL_STRING_num(cfg.skkeys);
	}

	if (keyidx < 0) {
		BIO_printf(bio_err, "No key specified\n");
		return (1);
	}

	if (cfg.key_param == NULL ||
	    cfg.key_param->idx != keyidx) {
		struct cms_key_param *nparam;

		if ((nparam = calloc(1, sizeof(struct cms_key_param))) == NULL)
			return (1);

		nparam->idx = keyidx;
		if ((nparam->param = sk_OPENSSL_STRING_new_null()) == NULL) {
			free(nparam);
			return (1);
		}

		nparam->next = NULL;
		if (cfg.key_first == NULL)
			cfg.key_first = nparam;
		else
			cfg.key_param->next = nparam;

		cfg.key_param = nparam;
	}

	if (!sk_OPENSSL_STRING_push(cfg.key_param->param, arg))
		return (1);

	return (0);
}

static int
cms_opt_md(char *arg)
{
	if ((cfg.sign_md = EVP_get_digestbyname(arg)) == NULL) {
		BIO_printf(bio_err, "Unknown digest %s\n", arg);
		return (1);
	}
	return (0);
}

static int
cms_opt_print(void)
{
	cfg.noout = 1;
	cfg.print = 1;
	return (0);
}

static int
cms_opt_pwri_pass(char *arg)
{
	cfg.pwri_pass = (unsigned char *)arg;
	return (0);
}

static int
cms_opt_recip(char *arg)
{
	if (cfg.operation == SMIME_ENCRYPT) {
		if (cfg.encerts == NULL) {
			if ((cfg.encerts = sk_X509_new_null()) == NULL)
				return (1);
		}

		cfg.cert = load_cert(bio_err, arg, FORMAT_PEM,
		    NULL, "recipient certificate file");
		if (cfg.cert == NULL)
			return (1);

		if (!sk_X509_push(cfg.encerts, cfg.cert))
			return (1);

		cfg.cert = NULL;
	} else {
		cfg.recipfile = arg;
	}
	return (0);
}

static int
cms_opt_receipt_request_from(char *arg)
{
	if (cfg.rr_from == NULL)
		cfg.rr_from = sk_OPENSSL_STRING_new_null();
	if (cfg.rr_from == NULL)
		return (1);
	if (!sk_OPENSSL_STRING_push(cfg.rr_from, arg))
		return (1);

	return (0);
}

static int
cms_opt_receipt_request_to(char *arg)
{
	if (cfg.rr_to == NULL)
		cfg.rr_to = sk_OPENSSL_STRING_new_null();
	if (cfg.rr_to == NULL)
		return (1);
	if (!sk_OPENSSL_STRING_push(cfg.rr_to, arg))
		return (1);

	return (0);
}

static int
cms_opt_secretkey(char *arg)
{
	long ltmp;

	free(cfg.secret_key);

	if ((cfg.secret_key = string_to_hex(arg, &ltmp)) == NULL) {
		BIO_printf(bio_err, "Invalid key %s\n", arg);
		return (1);
	}
	cfg.secret_keylen = (size_t)ltmp;
	return (0);
}

static int
cms_opt_secretkeyid(char *arg)
{
	long ltmp;

	free(cfg.secret_keyid);

	if ((cfg.secret_keyid = string_to_hex(arg, &ltmp)) == NULL) {
		BIO_printf(bio_err, "Invalid id %s\n", arg);
		return (1);
	}
	cfg.secret_keyidlen = (size_t)ltmp;
	return (0);
}

static int
cms_opt_signer(char *arg)
{
	if (cfg.signerfile == NULL) {
		cfg.signerfile = arg;
		return (0);
	}

	if (cfg.sksigners == NULL)
		cfg.sksigners = sk_OPENSSL_STRING_new_null();
	if (cfg.sksigners == NULL)
		return (1);
	if (!sk_OPENSSL_STRING_push(cfg.sksigners, cfg.signerfile))
		return (1);

	if (cfg.keyfile == NULL)
		cfg.keyfile = cfg.signerfile;

	if (cfg.skkeys == NULL)
		cfg.skkeys = sk_OPENSSL_STRING_new_null();
	if (cfg.skkeys == NULL)
		return (1);
	if (!sk_OPENSSL_STRING_push(cfg.skkeys, cfg.keyfile))
		return (1);

	cfg.keyfile = NULL;

	cfg.signerfile = arg;
	return (0);
}

static int
cms_opt_verify_param(int argc, char **argv, int *argsused)
{
	int oargc = argc;
	int badarg = 0;

	if (!args_verify(&argv, &argc, &badarg, bio_err, &cfg.vpm))
		return (1);
	if (badarg)
		return (1);

	*argsused = oargc - argc;

	return (0);
}

static int
cms_opt_verify_receipt(char *arg)
{
	cfg.operation = SMIME_VERIFY_RECEIPT;
	cfg.rctfile = arg;
	return (0);
}

static const struct option cms_options[] = {
#ifndef OPENSSL_NO_AES
	{
		.name = "aes128",
		.desc = "Encrypt PEM output with CBC AES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_cipher,
	},
	{
		.name = "aes192",
		.desc = "Encrypt PEM output with CBC AES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_cipher,
	},
	{
		.name = "aes256",
		.desc = "Encrypt PEM output with CBC AES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_cipher,
	},
#endif
#ifndef OPENSSL_NO_CAMELLIA
	{
		.name = "camellia128",
		.desc = "Encrypt PEM output with CBC Camellia",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_cipher,
	},
	{
		.name = "camellia192",
		.desc = "Encrypt PEM output with CBC Camellia",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_cipher,
	},
	{
		.name = "camellia256",
		.desc = "Encrypt PEM output with CBC Camellia",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_cipher,
	},
#endif
#ifndef OPENSSL_NO_DES
	{
		.name = "des",
		.desc = "Encrypt with DES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_cipher,
	},
	{
		.name = "des3",
		.desc = "Encrypt with triple DES (default)",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_cipher,
	},
#endif
#ifndef OPENSSL_NO_RC2
	{
		.name = "rc2-40",
		.desc = "Encrypt with RC2-40",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_cipher,
	},
	{
		.name = "rc2-64",
		.desc = "Encrypt with RC2-64",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_cipher,
	},
	{
		.name = "rc2-128",
		.desc = "Encrypt with RC2-128",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_cipher,
	},
#endif
	{
		.name = "CAfile",
		.argname = "file",
		.desc = "Certificate Authority file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.CAfile,
	},
	{
		.name = "CApath",
		.argname = "path",
		.desc = "Certificate Authority path",
		.type = OPTION_ARG,
		.opt.arg = &cfg.CApath,
	},
	{
		.name = "binary",
		.desc = "Do not translate message to text",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_BINARY,
	},
	{
		.name = "certfile",
		.argname = "file",
		.desc = "Other certificates file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.certfile,
	},
	{
		.name = "certsout",
		.argname = "file",
		.desc = "Certificate output file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.certsoutfile,
	},
	{
		.name = "cmsout",
		.desc = "Output CMS structure",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_CMSOUT,
	},
	{
		.name = "compress",
		.desc = "Create CMS CompressedData type",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_COMPRESS,
	},
	{
		.name = "content",
		.argname = "file",
		.desc = "Supply or override content for detached signature",
		.type = OPTION_ARG,
		.opt.arg = &cfg.contfile,
	},
	{
		.name = "crlfeol",
		.desc = "Use CRLF as EOL termination instead of CR only",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_CRLFEOL,
	},
	{
		.name = "data_create",
		.desc = "Create CMS Data type",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_DATA_CREATE,
	},
	{
		.name = "data_out",
		.desc = "Output content from the input CMS Data type",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_DATAOUT,
	},
	{
		.name = "debug_decrypt",
		.desc = "Set the CMS_DEBUG_DECRYPT flag when decrypting",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_DEBUG_DECRYPT,
	},
	{
		.name = "decrypt",
		.desc = "Decrypt encrypted message",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_DECRYPT,
	},
	{
		.name = "digest_create",
		.desc = "Create CMS DigestedData type",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_DIGEST_CREATE,
	},
	{
		.name = "digest_verify",
		.desc = "Verify CMS DigestedData type and output the content",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_DIGEST_VERIFY,
	},
	{
		.name = "econtent_type",
		.argname = "type",
		.desc = "Set the encapsulated content type",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = cms_opt_econtent_type,
	},
	{
		.name = "encrypt",
		.desc = "Encrypt message",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_ENCRYPT,
	},
	{
		.name = "EncryptedData_decrypt",
		.desc = "Decrypt CMS EncryptedData",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_ENCRYPTED_DECRYPT,
	},
	{
		.name = "EncryptedData_encrypt",
		.desc = "Encrypt content using supplied symmetric key and algorithm",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_ENCRYPTED_ENCRYPT,
	},
	{
		.name = "from",
		.argname = "addr",
		.desc = "From address",
		.type = OPTION_ARG,
		.opt.arg = &cfg.from,
	},
	{
		.name = "in",
		.argname = "file",
		.desc = "Input file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.infile,
	},
	{
		.name = "indef",
		.desc = "Same as -stream",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_STREAM,
	},
	{
		.name = "inform",
		.argname = "fmt",
		.desc = "Input format (DER, PEM or SMIME (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.informat,
	},
	{
		.name = "inkey",
		.argname = "file",
		.desc = "Input key file",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = cms_opt_inkey,
	},
	{
		.name = "keyform",
		.argname = "fmt",
		.desc = "Input key format (DER or PEM (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.keyform,
	},
	{
		.name = "keyid",
		.desc = "Use subject key identifier",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_USE_KEYID,
	},
	{
		.name = "keyopt",
		.argname = "nm:v",
		.desc = "Set public key parameters",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = cms_opt_keyopt,
	},
	{
		.name = "md",
		.argname = "digest",
		.desc = "Digest to use when signing or resigning",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = cms_opt_md,
	},
	{
		.name = "no_attr_verify",
		.desc = "Do not verify the signer's attribute of a signature",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_NO_ATTR_VERIFY,
	},
	{
		.name = "no_content_verify",
		.desc = "Do not verify the content of a signed message",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_NO_CONTENT_VERIFY,
	},
	{
		.name = "no_signer_cert_verify",
		.desc = "Do not verify the signer's certificate",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_NO_SIGNER_CERT_VERIFY,
	},
	{
		.name = "noattr",
		.desc = "Do not include any signed attributes",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_NOATTR,
	},
	{
		.name = "nocerts",
		.desc = "Do not include signer's certificate when signing",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_NOCERTS,
	},
	{
		.name = "nodetach",
		.desc = "Use opaque signing",
		.type = OPTION_VALUE_AND,
		.opt.value = &cfg.flags,
		.value = ~CMS_DETACHED,
	},
	{
		.name = "noindef",
		.desc = "Disable CMS streaming",
		.type = OPTION_VALUE_AND,
		.opt.value = &cfg.flags,
		.value = ~CMS_STREAM,
	},
	{
		.name = "nointern",
		.desc = "Do not search certificates in message for signer",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_NOINTERN,
	},
	{
		.name = "nooldmime",
		.desc = "Output old S/MIME content type",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_NOOLDMIMETYPE,
	},
	{
		.name = "noout",
		.desc = "Do not output the parsed CMS structure",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.noout,
	},
	{
		.name = "nosigs",
		.desc = "Do not verify message signature",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_NOSIGS,
	},
	{
		.name = "nosmimecap",
		.desc = "Omit the SMIMECapabilities attribute",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_NOSMIMECAP,
	},
	{
		.name = "noverify",
		.desc = "Do not verify signer's certificate",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_NO_SIGNER_CERT_VERIFY,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output file",
		.type = OPTION_ARG,
		.opt.arg = &cfg.outfile,
	},
	{
		.name = "outform",
		.argname = "fmt",
		.desc = "Output format (DER, PEM or SMIME (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.outformat,
	},
	{
		.name = "passin",
		.argname = "src",
		.desc = "Private key password source",
		.type = OPTION_ARG,
		.opt.arg = &cfg.passargin,
	},
	{
		.name = "print",
		.desc = "Print out all fields of the CMS structure for the -cmsout",
		.type = OPTION_FUNC,
		.opt.func = cms_opt_print,
	},
	{
		.name = "pwri_password",
		.argname = "arg",
		.desc = "Specify PasswordRecipientInfo (PWRI) password to use",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = cms_opt_pwri_pass,
	},
	{
		.name = "rctform",
		.argname = "fmt",
		.desc = "Receipt file format (DER, PEM or SMIME (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.rctformat,
	},
	{
		.name = "receipt_request_all",
		.desc = "Indicate requests should be provided by all recipients",
		.type = OPTION_VALUE,
		.opt.value = &cfg.rr_allorfirst,
		.value = 0,
	},
	{
		.name = "receipt_request_first",
		.desc = "Indicate requests should be provided by first tier recipient",
		.type = OPTION_VALUE,
		.opt.value = &cfg.rr_allorfirst,
		.value = 1,
	},
	{
		.name = "receipt_request_from",
		.argname = "addr",
		.desc = "Add explicit email address where receipts should be supplied",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = cms_opt_receipt_request_from,
	},
	{
		.name = "receipt_request_print",
		.desc = "Print out the contents of any signed receipt requests",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.rr_print,
	},
	{
		.name = "receipt_request_to",
		.argname = "addr",
		.desc = "Add explicit email address where receipts should be sent to",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = cms_opt_receipt_request_to,
	},
	{
		.name = "recip",
		.argname = "file",
		.desc = "Recipient certificate file for decryption",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = cms_opt_recip,
	},
	{
		.name = "resign",
		.desc = "Resign a signed message",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_RESIGN,
	},
	{
		.name = "secretkey",
		.argname = "key",
		.desc = "Specify symmetric key to use",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = cms_opt_secretkey,
	},
	{
		.name = "secretkeyid",
		.argname = "id",
		.desc = "The key identifier for the supplied symmetric key",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = cms_opt_secretkeyid,
	},
	{
		.name = "sign",
		.desc = "Sign message",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_SIGN,
	},
	{
		.name = "sign_receipt",
		.desc = "Generate a signed receipt for the message",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_SIGN_RECEIPT,
	},
	{
		.name = "signer",
		.argname = "file",
		.desc = "Signer certificate file",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = cms_opt_signer,
	},
	{
		.name = "stream",
		.desc = "Enable CMS streaming",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_STREAM,
	},
	{
		.name = "subject",
		.argname = "s",
		.desc = "Subject",
		.type = OPTION_ARG,
		.opt.arg = &cfg.subject,
	},
	{
		.name = "text",
		.desc = "Include or delete text MIME headers",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.flags,
		.value = CMS_TEXT,
	},
	{
		.name = "to",
		.argname = "addr",
		.desc = "To address",
		.type = OPTION_ARG,
		.opt.arg = &cfg.to,
	},
	{
		.name = "uncompress",
		.desc = "Uncompress CMS CompressedData type",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_UNCOMPRESS,
	},
	{
		.name = "verify",
		.desc = "Verify signed message",
		.type = OPTION_VALUE,
		.opt.value = &cfg.operation,
		.value = SMIME_VERIFY,
	},
	{
		.name = "verify_receipt",
		.argname = "file",
		.desc = "Verify a signed receipt in file",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = cms_opt_verify_receipt,
	},
	{
		.name = "verify_retcode",
		.desc = "Set verification error code to exit code",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.verify_retcode,
	},
	{
		.name = "check_ss_sig",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_verify_param,
	},
	{
		.name = "crl_check",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_verify_param,
	},
	{
		.name = "crl_check_all",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_verify_param,
	},
	{
		.name = "extended_crl",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_verify_param,
	},
	{
		.name = "ignore_critical",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_verify_param,
	},
	{
		.name = "issuer_checks",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_verify_param,
	},
	{
		.name = "policy",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_verify_param,
	},
	{
		.name = "policy_check",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_verify_param,
	},
	{
		.name = "purpose",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_verify_param,
	},
	{
		.name = "x509_strict",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_verify_param,
	},
	{
		.name = NULL,
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = cms_opt_cipher,
	},
	{ NULL },
};

static const struct option verify_shared_options[] = {
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
		.name = "extended_crl",
		.desc = "Enable extended CRL support",
	},
	{
		.name = "ignore_critical",
		.desc = "Disable critical extension checking",
	},
	{
		.name = "issuer_checks",
		.desc = "Enable debugging of certificate issuer checks",
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
		.name = "purpose",
		.argname = "name",
		.desc = "Verify for the given purpose",
	},
	{
		.name = "x509_strict",
		.desc = "Use strict X.509 rules (disables workarounds)",
	},
	{ NULL },
};

static void
cms_usage(void)
{
	int i;

	fprintf(stderr, "usage: cms "
	    "[-aes128 | -aes192 | -aes256 | -camellia128 |\n"
	    "    -camellia192 | -camellia256 | -des | -des3 |\n"
	    "    -rc2-40 | -rc2-64 | -rc2-128] [-CAfile file]\n"
	    "    [-CApath directory] [-binary] [-certfile file]\n"
	    "    [-certsout file] [-cmsout] [-compress] [-content file]\n"
	    "    [-crlfeol] [-data_create] [-data_out] [-debug_decrypt]\n"
	    "    [-decrypt] [-digest_create] [-digest_verify]\n"
	    "    [-econtent_type type] [-encrypt] [-EncryptedData_decrypt]\n"
	    "    [-EncryptedData_encrypt] [-from addr] [-in file]\n"
	    "    [-inform der | pem | smime] [-inkey file]\n"
	    "    [-keyform der | pem] [-keyid] [-keyopt nm:v] [-md digest]\n"
	    "    [-no_attr_verify] [-no_content_verify]\n"
	    "    [-no_signer_cert_verify] [-noattr] [-nocerts] [-nodetach]\n"
	    "    [-nointern] [-nooldmime] [-noout] [-nosigs] [-nosmimecap]\n"
	    "    [-noverify] [-out file] [-outform der | pem | smime]\n"
	    "    [-passin src] [-print] [-pwri_password arg]\n"
	    "    [-rctform der | pem | smime]\n"
	    "    [-receipt_request_all | -receipt_request_first]\n"
	    "    [-receipt_request_from addr] [-receipt_request_print]\n"
	    "    [-receipt_request_to addr] [-recip file] [-resign]\n"
	    "    [-secretkey key] [-secretkeyid id] [-sign] [-sign_receipt]\n"
	    "    [-signer file] [-stream | -indef | -noindef] [-subject s]\n"
	    "    [-text] [-to addr] [-uncompress] [-verify]\n"
	    "    [-verify_receipt file] [-verify_retcode] [cert.pem ...]\n\n");

	options_usage(cms_options);

	fprintf(stderr, "\nVerification options:\n\n");
	options_usage(verify_shared_options);

	fprintf(stderr, "\nValid purposes:\n\n");
	for (i = 0; i < X509_PURPOSE_get_count(); i++) {
		const X509_PURPOSE *ptmp = X509_PURPOSE_get0(i);
		fprintf(stderr, "  %-18s%s\n", X509_PURPOSE_get0_sname(ptmp),
		    X509_PURPOSE_get0_name(ptmp));
	}
}

int
cms_main(int argc, char **argv)
{
	int ret = 0;
	char **args;
	int argsused = 0;
	const char *inmode = "r", *outmode = "w";
	CMS_ContentInfo *cms = NULL, *rcms = NULL;
	X509_STORE *store = NULL;
	X509 *recip = NULL, *signer = NULL;
	EVP_PKEY *key = NULL;
	STACK_OF(X509) *other = NULL;
	BIO *in = NULL, *out = NULL, *indata = NULL, *rctin = NULL;
	int badarg = 0;
	CMS_ReceiptRequest *rr = NULL;
	char *passin = NULL;
	unsigned char *pwri_tmp = NULL;

	if (pledge("stdio rpath wpath cpath tty", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.flags = CMS_DETACHED;
	cfg.rr_allorfirst = -1;
	cfg.informat = FORMAT_SMIME;
	cfg.outformat = FORMAT_SMIME;
	cfg.rctformat = FORMAT_SMIME;
	cfg.keyform = FORMAT_PEM;
	if (options_parse(argc, argv, cms_options, NULL, &argsused) != 0) {
		goto argerr;
	}
	args = argv + argsused;
	ret = 1;

	if (((cfg.rr_allorfirst != -1) || cfg.rr_from != NULL) &&
	    cfg.rr_to == NULL) {
		BIO_puts(bio_err, "No Signed Receipts Recipients\n");
		goto argerr;
	}
	if (!(cfg.operation & SMIME_SIGNERS) &&
	    (cfg.rr_to != NULL || cfg.rr_from != NULL)) {
		BIO_puts(bio_err, "Signed receipts only allowed with -sign\n");
		goto argerr;
	}
	if (!(cfg.operation & SMIME_SIGNERS) &&
	    (cfg.skkeys != NULL || cfg.sksigners != NULL)) {
		BIO_puts(bio_err, "Multiple signers or keys not allowed\n");
		goto argerr;
	}
	if (cfg.operation & SMIME_SIGNERS) {
		if (cfg.keyfile != NULL &&
		    cfg.signerfile == NULL) {
			BIO_puts(bio_err, "Illegal -inkey without -signer\n");
			goto argerr;
		}
		/* Check to see if any final signer needs to be appended */
		if (cfg.signerfile != NULL) {
			if (cfg.sksigners == NULL &&
			    (cfg.sksigners =
			    sk_OPENSSL_STRING_new_null()) == NULL)
				goto end;
			if (!sk_OPENSSL_STRING_push(cfg.sksigners,
			    cfg.signerfile))
				goto end;
			if (cfg.skkeys == NULL &&
			    (cfg.skkeys =
			    sk_OPENSSL_STRING_new_null()) == NULL)
				goto end;
			if (cfg.keyfile == NULL)
				cfg.keyfile = cfg.signerfile;
			if (!sk_OPENSSL_STRING_push(cfg.skkeys,
			    cfg.keyfile))
				goto end;
		}
		if (cfg.sksigners == NULL) {
			BIO_printf(bio_err,
			    "No signer certificate specified\n");
			badarg = 1;
		}
		cfg.signerfile = NULL;
		cfg.keyfile = NULL;
	} else if (cfg.operation == SMIME_DECRYPT) {
		if (cfg.recipfile == NULL &&
		    cfg.keyfile == NULL &&
		    cfg.secret_key == NULL &&
		    cfg.pwri_pass == NULL) {
			BIO_printf(bio_err,
			    "No recipient certificate or key specified\n");
			badarg = 1;
		}
	} else if (cfg.operation == SMIME_ENCRYPT) {
		if (*args == NULL && cfg.secret_key == NULL &&
		    cfg.pwri_pass == NULL &&
		    cfg.encerts == NULL) {
			BIO_printf(bio_err,
			    "No recipient(s) certificate(s) specified\n");
			badarg = 1;
		}
	} else if (!cfg.operation) {
		badarg = 1;
	}

	if (badarg) {
 argerr:
		cms_usage();
		goto end;
	}

	if (!app_passwd(bio_err, cfg.passargin, NULL, &passin, NULL)) {
		BIO_printf(bio_err, "Error getting password\n");
		goto end;
	}
	ret = 2;

	if (!(cfg.operation & SMIME_SIGNERS))
		cfg.flags &= ~CMS_DETACHED;

	if (cfg.operation & SMIME_OP) {
		if (cfg.outformat == FORMAT_ASN1)
			outmode = "wb";
	} else {
		if (cfg.flags & CMS_BINARY)
			outmode = "wb";
	}

	if (cfg.operation & SMIME_IP) {
		if (cfg.informat == FORMAT_ASN1)
			inmode = "rb";
	} else {
		if (cfg.flags & CMS_BINARY)
			inmode = "rb";
	}

	if (cfg.operation == SMIME_ENCRYPT) {
		if (cfg.cipher == NULL) {
#ifndef OPENSSL_NO_DES
			cfg.cipher = EVP_des_ede3_cbc();
#else
			BIO_printf(bio_err, "No cipher selected\n");
			goto end;
#endif
		}
		if (cfg.secret_key != NULL &&
		    cfg.secret_keyid == NULL) {
			BIO_printf(bio_err, "No secret key id\n");
			goto end;
		}
		if (*args != NULL && cfg.encerts == NULL)
			if ((cfg.encerts = sk_X509_new_null()) == NULL)
				goto end;
		while (*args) {
			if ((cfg.cert = load_cert(bio_err, *args,
			    FORMAT_PEM, NULL,
			    "recipient certificate file")) == NULL)
				goto end;
			if (!sk_X509_push(cfg.encerts, cfg.cert))
				goto end;
			cfg.cert = NULL;
			args++;
		}
	}
	if (cfg.certfile != NULL) {
		if ((other = load_certs(bio_err, cfg.certfile,
		    FORMAT_PEM, NULL, "certificate file")) == NULL) {
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	if (cfg.recipfile != NULL &&
	    (cfg.operation == SMIME_DECRYPT)) {
		if ((recip = load_cert(bio_err, cfg.recipfile,
		    FORMAT_PEM, NULL, "recipient certificate file")) == NULL) {
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	if (cfg.operation == SMIME_SIGN_RECEIPT) {
		if ((signer = load_cert(bio_err, cfg.signerfile,
		    FORMAT_PEM, NULL,
		    "receipt signer certificate file")) == NULL) {
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	if (cfg.operation == SMIME_DECRYPT) {
		if (cfg.keyfile == NULL)
			cfg.keyfile = cfg.recipfile;
	} else if ((cfg.operation == SMIME_SIGN) ||
	    (cfg.operation == SMIME_SIGN_RECEIPT)) {
		if (cfg.keyfile == NULL)
			cfg.keyfile = cfg.signerfile;
	} else {
		cfg.keyfile = NULL;
	}

	if (cfg.keyfile != NULL) {
		key = load_key(bio_err, cfg.keyfile, cfg.keyform,
		    0, passin, "signing key file");
		if (key == NULL)
			goto end;
	}
	if (cfg.infile != NULL) {
		if ((in = BIO_new_file(cfg.infile, inmode)) == NULL) {
			BIO_printf(bio_err,
			    "Can't open input file %s\n", cfg.infile);
			goto end;
		}
	} else {
		if ((in = BIO_new_fp(stdin, BIO_NOCLOSE)) == NULL)
			goto end;
	}

	if (cfg.operation & SMIME_IP) {
		if (cfg.informat == FORMAT_SMIME)
			cms = SMIME_read_CMS(in, &indata);
		else if (cfg.informat == FORMAT_PEM)
			cms = PEM_read_bio_CMS(in, NULL, NULL, NULL);
		else if (cfg.informat == FORMAT_ASN1)
			cms = d2i_CMS_bio(in, NULL);
		else {
			BIO_printf(bio_err, "Bad input format for CMS file\n");
			goto end;
		}

		if (cms == NULL) {
			BIO_printf(bio_err, "Error reading S/MIME message\n");
			goto end;
		}
		if (cfg.contfile != NULL) {
			BIO_free(indata);
			if ((indata = BIO_new_file(cfg.contfile,
			    "rb")) == NULL) {
				BIO_printf(bio_err,
				    "Can't read content file %s\n",
				    cfg.contfile);
				goto end;
			}
		}
		if (cfg.certsoutfile != NULL) {
			STACK_OF(X509) *allcerts;
			if ((allcerts = CMS_get1_certs(cms)) == NULL)
				goto end;
			if (!save_certs(cfg.certsoutfile, allcerts)) {
				BIO_printf(bio_err,
				    "Error writing certs to %s\n",
				    cfg.certsoutfile);
				sk_X509_pop_free(allcerts, X509_free);
				ret = 5;
				goto end;
			}
			sk_X509_pop_free(allcerts, X509_free);
		}
	}
	if (cfg.rctfile != NULL) {
		char *rctmode = (cfg.rctformat == FORMAT_ASN1) ?
		    "rb" : "r";
		if ((rctin = BIO_new_file(cfg.rctfile, rctmode)) == NULL) {
			BIO_printf(bio_err,
			    "Can't open receipt file %s\n", cfg.rctfile);
			goto end;
		}
		if (cfg.rctformat == FORMAT_SMIME)
			rcms = SMIME_read_CMS(rctin, NULL);
		else if (cfg.rctformat == FORMAT_PEM)
			rcms = PEM_read_bio_CMS(rctin, NULL, NULL, NULL);
		else if (cfg.rctformat == FORMAT_ASN1)
			rcms = d2i_CMS_bio(rctin, NULL);
		else {
			BIO_printf(bio_err, "Bad input format for receipt\n");
			goto end;
		}

		if (rcms == NULL) {
			BIO_printf(bio_err, "Error reading receipt\n");
			goto end;
		}
	}
	if (cfg.outfile != NULL) {
		if ((out = BIO_new_file(cfg.outfile, outmode)) == NULL) {
			BIO_printf(bio_err,
			    "Can't open output file %s\n", cfg.outfile);
			goto end;
		}
	} else {
		if ((out = BIO_new_fp(stdout, BIO_NOCLOSE)) == NULL)
			goto end;
	}

	if ((cfg.operation == SMIME_VERIFY) ||
	    (cfg.operation == SMIME_VERIFY_RECEIPT)) {
		if ((store = setup_verify(bio_err, cfg.CAfile,
		    cfg.CApath)) == NULL)
			goto end;
		if (cfg.vpm != NULL) {
			if (!X509_STORE_set1_param(store, cfg.vpm))
				goto end;
		}
	}
	ret = 3;

	if (cfg.operation == SMIME_DATA_CREATE) {
		cms = CMS_data_create(in, cfg.flags);
	} else if (cfg.operation == SMIME_DIGEST_CREATE) {
		cms = CMS_digest_create(in, cfg.sign_md,
		    cfg.flags);
	} else if (cfg.operation == SMIME_COMPRESS) {
		cms = CMS_compress(in, -1, cfg.flags);
	} else if (cfg.operation == SMIME_ENCRYPT) {
		int i;
		cfg.flags |= CMS_PARTIAL;
		cms = CMS_encrypt(NULL, in, cfg.cipher,
		    cfg.flags);
		if (cms == NULL)
			goto end;
		for (i = 0; i < sk_X509_num(cfg.encerts); i++) {
			CMS_RecipientInfo *ri;
			struct cms_key_param *kparam;
			int tflags = cfg.flags;
			X509 *x;

			if ((x = sk_X509_value(cfg.encerts, i)) == NULL)
				goto end;
			for (kparam = cfg.key_first; kparam != NULL;
			    kparam = kparam->next) {
				if (kparam->idx == i) {
					tflags |= CMS_KEY_PARAM;
					break;
				}
			}
			ri = CMS_add1_recipient_cert(cms, x, tflags);
			if (ri == NULL)
				goto end;
			if (kparam != NULL) {
				EVP_PKEY_CTX *pctx;
				if ((pctx = CMS_RecipientInfo_get0_pkey_ctx(
				    ri)) == NULL)
					goto end;
				if (!cms_set_pkey_param(pctx, kparam->param))
					goto end;
			}
		}

		if (cfg.secret_key != NULL) {
			if (CMS_add0_recipient_key(cms, NID_undef,
			    cfg.secret_key, cfg.secret_keylen,
			    cfg.secret_keyid, cfg.secret_keyidlen,
			    NULL, NULL, NULL) == NULL)
				goto end;
			/* NULL these because call absorbs them */
			cfg.secret_key = NULL;
			cfg.secret_keyid = NULL;
		}
		if (cfg.pwri_pass != NULL) {
			pwri_tmp = strdup(cfg.pwri_pass);
			if (pwri_tmp == NULL)
				goto end;
			if (CMS_add0_recipient_password(cms, -1, NID_undef,
			    NID_undef, pwri_tmp, -1, NULL) == NULL)
				goto end;
			pwri_tmp = NULL;
		}
		if (!(cfg.flags & CMS_STREAM)) {
			if (!CMS_final(cms, in, NULL, cfg.flags))
				goto end;
		}
	} else if (cfg.operation == SMIME_ENCRYPTED_ENCRYPT) {
		cms = CMS_EncryptedData_encrypt(in, cfg.cipher,
		    cfg.secret_key, cfg.secret_keylen,
		    cfg.flags);

	} else if (cfg.operation == SMIME_SIGN_RECEIPT) {
		CMS_ContentInfo *srcms = NULL;
		STACK_OF(CMS_SignerInfo) *sis;
		CMS_SignerInfo *si;
		sis = CMS_get0_SignerInfos(cms);
		if (sis == NULL)
			goto end;
		si = sk_CMS_SignerInfo_value(sis, 0);
		if (si == NULL)
			goto end;
		srcms = CMS_sign_receipt(si, signer, key, other,
		    cfg.flags);
		if (srcms == NULL)
			goto end;
		CMS_ContentInfo_free(cms);
		cms = srcms;
	} else if (cfg.operation & SMIME_SIGNERS) {
		int i;
		/*
		 * If detached data content we enable streaming if S/MIME
		 * output format.
		 */
		if (cfg.operation == SMIME_SIGN) {

			if (cfg.flags & CMS_DETACHED) {
				if (cfg.outformat == FORMAT_SMIME)
					cfg.flags |= CMS_STREAM;
			}
			cfg.flags |= CMS_PARTIAL;
			cms = CMS_sign(NULL, NULL, other, in, cfg.flags);
			if (cms == NULL)
				goto end;
			if (cfg.econtent_type != NULL)
				if (!CMS_set1_eContentType(cms,
				    cfg.econtent_type))
					goto end;

			if (cfg.rr_to != NULL) {
				rr = make_receipt_request(cfg.rr_to,
				    cfg.rr_allorfirst,
				    cfg.rr_from);
				if (rr == NULL) {
					BIO_puts(bio_err,
					    "Signed Receipt Request Creation Error\n");
					goto end;
				}
			}
		} else {
			cfg.flags |= CMS_REUSE_DIGEST;
		}

		for (i = 0; i < sk_OPENSSL_STRING_num(cfg.sksigners); i++) {
			CMS_SignerInfo *si;
			struct cms_key_param *kparam;
			int tflags = cfg.flags;

			cfg.signerfile = sk_OPENSSL_STRING_value(
			    cfg.sksigners, i);
			cfg.keyfile = sk_OPENSSL_STRING_value(
			    cfg.skkeys, i);

			signer = load_cert(bio_err, cfg.signerfile,
			    FORMAT_PEM, NULL, "signer certificate");
			if (signer == NULL)
				goto end;
			key = load_key(bio_err, cfg.keyfile,
			    cfg.keyform, 0, passin, "signing key file");
			if (key == NULL)
				goto end;
			for (kparam = cfg.key_first; kparam != NULL;
			    kparam = kparam->next) {
				if (kparam->idx == i) {
					tflags |= CMS_KEY_PARAM;
					break;
				}
			}
			si = CMS_add1_signer(cms, signer, key,
			    cfg.sign_md, tflags);
			if (si == NULL)
				goto end;
			if (kparam != NULL) {
				EVP_PKEY_CTX *pctx;
				if ((pctx = CMS_SignerInfo_get0_pkey_ctx(
				    si)) == NULL)
					goto end;
				if (!cms_set_pkey_param(pctx, kparam->param))
					goto end;
			}
			if (rr != NULL && !CMS_add1_ReceiptRequest(si, rr))
				goto end;
			X509_free(signer);
			signer = NULL;
			EVP_PKEY_free(key);
			key = NULL;
		}
		/* If not streaming or resigning finalize structure */
		if ((cfg.operation == SMIME_SIGN) &&
		    !(cfg.flags & CMS_STREAM)) {
			if (!CMS_final(cms, in, NULL, cfg.flags))
				goto end;
		}
	}
	if (cms == NULL) {
		BIO_printf(bio_err, "Error creating CMS structure\n");
		goto end;
	}
	ret = 4;
	if (cfg.operation == SMIME_DECRYPT) {
		if (cfg.flags & CMS_DEBUG_DECRYPT)
			CMS_decrypt(cms, NULL, NULL, NULL, NULL,
			    cfg.flags);

		if (cfg.secret_key != NULL) {
			if (!CMS_decrypt_set1_key(cms, cfg.secret_key,
			    cfg.secret_keylen, cfg.secret_keyid,
			    cfg.secret_keyidlen)) {
				BIO_puts(bio_err,
				    "Error decrypting CMS using secret key\n");
				goto end;
			}
		}
		if (key != NULL) {
			if (!CMS_decrypt_set1_pkey(cms, key, recip)) {
				BIO_puts(bio_err,
				    "Error decrypting CMS using private key\n");
				goto end;
			}
		}
		if (cfg.pwri_pass != NULL) {
			if (!CMS_decrypt_set1_password(cms,
			    cfg.pwri_pass, -1)) {
				BIO_puts(bio_err,
				    "Error decrypting CMS using password\n");
				goto end;
			}
		}
		if (!CMS_decrypt(cms, NULL, NULL, indata, out,
		    cfg.flags)) {
			BIO_printf(bio_err, "Error decrypting CMS structure\n");
			goto end;
		}
	} else if (cfg.operation == SMIME_DATAOUT) {
		if (!CMS_data(cms, out, cfg.flags))
			goto end;
	} else if (cfg.operation == SMIME_UNCOMPRESS) {
		if (!CMS_uncompress(cms, indata, out, cfg.flags))
			goto end;
	} else if (cfg.operation == SMIME_DIGEST_VERIFY) {
		if (CMS_digest_verify(cms, indata, out, cfg.flags) > 0)
			BIO_printf(bio_err, "Verification successful\n");
		else {
			BIO_printf(bio_err, "Verification failure\n");
			goto end;
		}
	} else if (cfg.operation == SMIME_ENCRYPTED_DECRYPT) {
		if (!CMS_EncryptedData_decrypt(cms, cfg.secret_key,
		    cfg.secret_keylen, indata, out, cfg.flags))
			goto end;
	} else if (cfg.operation == SMIME_VERIFY) {
		if (CMS_verify(cms, other, store, indata, out,
		    cfg.flags) > 0) {
			BIO_printf(bio_err, "Verification successful\n");
		} else {
			BIO_printf(bio_err, "Verification failure\n");
			if (cfg.verify_retcode)
				ret = verify_err + 32;
			goto end;
		}
		if (cfg.signerfile != NULL) {
			STACK_OF(X509) *signers;
			if ((signers = CMS_get0_signers(cms)) == NULL)
				goto end;
			if (!save_certs(cfg.signerfile, signers)) {
				BIO_printf(bio_err,
				    "Error writing signers to %s\n",
				    cfg.signerfile);
				sk_X509_free(signers);
				ret = 5;
				goto end;
			}
			sk_X509_free(signers);
		}
		if (cfg.rr_print)
			receipt_request_print(bio_err, cms);

	} else if (cfg.operation == SMIME_VERIFY_RECEIPT) {
		if (CMS_verify_receipt(rcms, cms, other, store,
		    cfg.flags) > 0) {
			BIO_printf(bio_err, "Verification successful\n");
		} else {
			BIO_printf(bio_err, "Verification failure\n");
			goto end;
		}
	} else {
		if (cfg.noout) {
			if (cfg.print &&
			    !CMS_ContentInfo_print_ctx(out, cms, 0, NULL))
				goto end;
		} else if (cfg.outformat == FORMAT_SMIME) {
			if (cfg.to != NULL)
				BIO_printf(out, "To: %s\n", cfg.to);
			if (cfg.from != NULL)
				BIO_printf(out, "From: %s\n", cfg.from);
			if (cfg.subject != NULL)
				BIO_printf(out, "Subject: %s\n",
				    cfg.subject);
			if (cfg.operation == SMIME_RESIGN)
				ret = SMIME_write_CMS(out, cms, indata,
				    cfg.flags);
			else
				ret = SMIME_write_CMS(out, cms, in,
				    cfg.flags);
		} else if (cfg.outformat == FORMAT_PEM) {
			ret = PEM_write_bio_CMS_stream(out, cms, in,
			    cfg.flags);
		} else if (cfg.outformat == FORMAT_ASN1) {
			ret = i2d_CMS_bio_stream(out, cms, in, cfg.flags);
		} else {
			BIO_printf(bio_err, "Bad output format for CMS file\n");
			goto end;
		}
		if (ret <= 0) {
			ret = 6;
			goto end;
		}
	}
	ret = 0;

 end:
	if (ret)
		ERR_print_errors(bio_err);

	sk_X509_pop_free(cfg.encerts, X509_free);
	sk_X509_pop_free(other, X509_free);
	X509_VERIFY_PARAM_free(cfg.vpm);
	sk_OPENSSL_STRING_free(cfg.sksigners);
	sk_OPENSSL_STRING_free(cfg.skkeys);
	free(cfg.secret_key);
	free(cfg.secret_keyid);
	free(pwri_tmp);
	ASN1_OBJECT_free(cfg.econtent_type);
	CMS_ReceiptRequest_free(rr);
	sk_OPENSSL_STRING_free(cfg.rr_to);
	sk_OPENSSL_STRING_free(cfg.rr_from);
	for (cfg.key_param = cfg.key_first; cfg.key_param;) {
		struct cms_key_param *tparam;
		sk_OPENSSL_STRING_free(cfg.key_param->param);
		tparam = cfg.key_param->next;
		free(cfg.key_param);
		cfg.key_param = tparam;
	}
	X509_STORE_free(store);
	X509_free(cfg.cert);
	X509_free(recip);
	X509_free(signer);
	EVP_PKEY_free(key);
	CMS_ContentInfo_free(cms);
	CMS_ContentInfo_free(rcms);
	BIO_free(rctin);
	BIO_free(in);
	BIO_free(indata);
	BIO_free_all(out);
	free(passin);

	return (ret);
}

static int
save_certs(char *signerfile, STACK_OF(X509) *signers)
{
	int i;
	BIO *tmp;

	if (signerfile == NULL)
		return 1;
	tmp = BIO_new_file(signerfile, "w");
	if (tmp == NULL)
		return 0;
	for (i = 0; i < sk_X509_num(signers); i++)
		PEM_write_bio_X509(tmp, sk_X509_value(signers, i));
	BIO_free(tmp);
	return 1;
}

static void
gnames_stack_print(BIO *out, STACK_OF(GENERAL_NAMES) *gns)
{
	STACK_OF(GENERAL_NAME) *gens;
	GENERAL_NAME *gen;
	int i, j;

	for (i = 0; i < sk_GENERAL_NAMES_num(gns); i++) {
		gens = sk_GENERAL_NAMES_value(gns, i);
		for (j = 0; j < sk_GENERAL_NAME_num(gens); j++) {
			gen = sk_GENERAL_NAME_value(gens, j);
			BIO_puts(out, "    ");
			GENERAL_NAME_print(out, gen);
			BIO_puts(out, "\n");
		}
	}
	return;
}

static void
receipt_request_print(BIO *out, CMS_ContentInfo *cms)
{
	STACK_OF(CMS_SignerInfo) *sis;
	CMS_SignerInfo *si;
	CMS_ReceiptRequest *rr;
	int allorfirst;
	STACK_OF(GENERAL_NAMES) *rto, *rlist;
	ASN1_STRING *scid;
	int i, rv;

	if ((sis = CMS_get0_SignerInfos(cms)) == NULL)
		return;
	for (i = 0; i < sk_CMS_SignerInfo_num(sis); i++) {
		if ((si = sk_CMS_SignerInfo_value(sis, i)) == NULL)
			return;
		rv = CMS_get1_ReceiptRequest(si, &rr);
		BIO_printf(bio_err, "Signer %d:\n", i + 1);
		if (rv == 0) {
			BIO_puts(bio_err, "  No Receipt Request\n");
		} else if (rv < 0) {
			BIO_puts(bio_err, "  Receipt Request Parse Error\n");
			ERR_print_errors(bio_err);
		} else {
			char *id;
			int idlen;

			CMS_ReceiptRequest_get0_values(rr, &scid, &allorfirst,
			    &rlist, &rto);
			BIO_puts(out, "  Signed Content ID:\n");
			idlen = ASN1_STRING_length(scid);
			id = (char *) ASN1_STRING_data(scid);
			BIO_dump_indent(out, id, idlen, 4);
			BIO_puts(out, "  Receipts From");
			if (rlist != NULL) {
				BIO_puts(out, " List:\n");
				gnames_stack_print(out, rlist);
			} else if (allorfirst == 1) {
				BIO_puts(out, ": First Tier\n");
			} else if (allorfirst == 0) {
				BIO_puts(out, ": All\n");
			} else {
				BIO_printf(out, " Unknown (%d)\n", allorfirst);
			}
			BIO_puts(out, "  Receipts To:\n");
			gnames_stack_print(out, rto);
		}
		CMS_ReceiptRequest_free(rr);
	}
}

static STACK_OF(GENERAL_NAMES) *
make_names_stack(STACK_OF(OPENSSL_STRING) *ns)
{
	int i;
	STACK_OF(GENERAL_NAMES) *ret;
	GENERAL_NAMES *gens = NULL;
	GENERAL_NAME *gen = NULL;

	if ((ret = sk_GENERAL_NAMES_new_null()) == NULL)
		goto err;
	for (i = 0; i < sk_OPENSSL_STRING_num(ns); i++) {
		char *str = sk_OPENSSL_STRING_value(ns, i);
		gen = a2i_GENERAL_NAME(NULL, NULL, NULL, GEN_EMAIL, str, 0);
		if (gen == NULL)
			goto err;
		gens = GENERAL_NAMES_new();
		if (gens == NULL)
			goto err;
		if (!sk_GENERAL_NAME_push(gens, gen))
			goto err;
		gen = NULL;
		if (!sk_GENERAL_NAMES_push(ret, gens))
			goto err;
		gens = NULL;
	}

	return ret;

 err:
	sk_GENERAL_NAMES_pop_free(ret, GENERAL_NAMES_free);
	GENERAL_NAMES_free(gens);
	GENERAL_NAME_free(gen);

	return NULL;
}


static CMS_ReceiptRequest *
make_receipt_request(STACK_OF(OPENSSL_STRING) *rr_to, int rr_allorfirst,
    STACK_OF(OPENSSL_STRING) *rr_from)
{
	STACK_OF(GENERAL_NAMES) *rct_to = NULL, *rct_from = NULL;
	CMS_ReceiptRequest *rr;

	rct_to = make_names_stack(rr_to);
	if (rct_to == NULL)
		goto err;
	if (rr_from != NULL) {
		rct_from = make_names_stack(rr_from);
		if (rct_from == NULL)
			goto err;
	} else {
		rct_from = NULL;
	}

	if ((rr = CMS_ReceiptRequest_create0(NULL, -1, rr_allorfirst, rct_from,
	    rct_to)) == NULL)
		goto err;

	return rr;

 err:
	sk_GENERAL_NAMES_pop_free(rct_to, GENERAL_NAMES_free);
	sk_GENERAL_NAMES_pop_free(rct_from, GENERAL_NAMES_free);
	return NULL;
}

static int
cms_set_pkey_param(EVP_PKEY_CTX *pctx, STACK_OF(OPENSSL_STRING) *param)
{
	char *keyopt;
	int i;

	if (sk_OPENSSL_STRING_num(param) <= 0)
		return 1;
	for (i = 0; i < sk_OPENSSL_STRING_num(param); i++) {
		keyopt = sk_OPENSSL_STRING_value(param, i);
		if (pkey_ctrl_string(pctx, keyopt) <= 0) {
			BIO_printf(bio_err, "parameter error \"%s\"\n", keyopt);
			ERR_print_errors(bio_err);
			return 0;
		}
	}
	return 1;
}

#endif
