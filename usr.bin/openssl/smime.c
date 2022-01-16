/* $OpenBSD: smime.c,v 1.17 2022/01/16 07:12:28 inoguchi Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 1999-2004 The OpenSSL Project.  All rights reserved.
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

/* S/MIME utility function */

#include <stdio.h>
#include <string.h>

#include "apps.h"

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

static int save_certs(char *signerfile, STACK_OF(X509) *signers);
static int smime_cb(int ok, X509_STORE_CTX *ctx);

#define SMIME_OP	0x10
#define SMIME_IP	0x20
#define SMIME_SIGNERS	0x40
#define SMIME_ENCRYPT	(1 | SMIME_OP)
#define SMIME_DECRYPT	(2 | SMIME_IP)
#define SMIME_SIGN	(3 | SMIME_OP | SMIME_SIGNERS)
#define SMIME_VERIFY	(4 | SMIME_IP)
#define SMIME_PK7OUT	(5 | SMIME_IP | SMIME_OP)
#define SMIME_RESIGN	(6 | SMIME_IP | SMIME_OP | SMIME_SIGNERS)

static struct {
	char *CAfile;
	char *CApath;
	char *certfile;
	const EVP_CIPHER *cipher;
	char *contfile;
	int flags;
	char *from;
	int indef;
	char *infile;
	int informat;
	char *keyfile;
	int keyform;
	int operation;
	char *outfile;
	int outformat;
	char *passargin;
	char *recipfile;
	const EVP_MD *sign_md;
	char *signerfile;
	STACK_OF(OPENSSL_STRING) *skkeys;
	STACK_OF(OPENSSL_STRING) *sksigners;
	char *subject;
	char *to;
	X509_VERIFY_PARAM *vpm;
} smime_config;

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
		return NULL;
}

static int
smime_opt_cipher(int argc, char **argv, int *argsused)
{
	char *name = argv[0];

	if (*name++ != '-')
		return (1);

	if ((smime_config.cipher = get_cipher_by_name(name)) == NULL)
		if ((smime_config.cipher = EVP_get_cipherbyname(name)) == NULL)
			return (1);

	*argsused = 1;
	return (0);
}

static int
smime_opt_inkey(char *arg)
{
	if (smime_config.keyfile == NULL) {
		smime_config.keyfile = arg;
		return (0);
	}

	if (smime_config.signerfile == NULL) {
		BIO_puts(bio_err, "Illegal -inkey without -signer\n");
		return (1);
	}

	if (smime_config.sksigners == NULL) {
		if ((smime_config.sksigners = sk_OPENSSL_STRING_new_null()) == NULL)
			return (1);
	}
	if (!sk_OPENSSL_STRING_push(smime_config.sksigners,
	    smime_config.signerfile))
		return (1);

	smime_config.signerfile = NULL;

	if (smime_config.skkeys == NULL) {
		if ((smime_config.skkeys = sk_OPENSSL_STRING_new_null()) == NULL)
			return (1);
	}
	if (!sk_OPENSSL_STRING_push(smime_config.skkeys, smime_config.keyfile))
		return (1);

	smime_config.keyfile = arg;
	return (0);
}

static int
smime_opt_md(char *arg)
{
	if ((smime_config.sign_md = EVP_get_digestbyname(arg)) == NULL) {
		BIO_printf(bio_err, "Unknown digest %s\n", arg);
		return (1);
	}
	return (0);
}

static int
smime_opt_signer(char *arg)
{
	if (smime_config.signerfile == NULL) {
		smime_config.signerfile = arg;
		return (0);
	}

	if (smime_config.sksigners == NULL) {
		if ((smime_config.sksigners = sk_OPENSSL_STRING_new_null()) == NULL)
			return (1);
	}
	if (!sk_OPENSSL_STRING_push(smime_config.sksigners,
	    smime_config.signerfile))
		return (1);

	if (smime_config.keyfile == NULL)
		smime_config.keyfile = smime_config.signerfile;

	if (smime_config.skkeys == NULL) {
		if ((smime_config.skkeys = sk_OPENSSL_STRING_new_null()) == NULL)
			return (1);
	}
	if (!sk_OPENSSL_STRING_push(smime_config.skkeys, smime_config.keyfile))
		return (1);

	smime_config.keyfile = NULL;

	smime_config.signerfile = arg;
	return (0);
}

static int
smime_opt_verify_param(int argc, char **argv, int *argsused)
{
	int oargc = argc;
	int badarg = 0;

	if (!args_verify(&argv, &argc, &badarg, bio_err, &smime_config.vpm))
		return (1);
	if (badarg)
		return (1);

	*argsused = oargc - argc;

	return (0);
}

static const struct option smime_options[] = {
#ifndef OPENSSL_NO_AES
	{
		.name = "aes128",
		.desc = "Encrypt PEM output with CBC AES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_cipher,
	},
	{
		.name = "aes192",
		.desc = "Encrypt PEM output with CBC AES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_cipher,
	},
	{
		.name = "aes256",
		.desc = "Encrypt PEM output with CBC AES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_cipher,
	},
#endif
#ifndef OPENSSL_NO_CAMELLIA
	{
		.name = "camellia128",
		.desc = "Encrypt PEM output with CBC Camellia",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_cipher,
	},
	{
		.name = "camellia192",
		.desc = "Encrypt PEM output with CBC Camellia",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_cipher,
	},
	{
		.name = "camellia256",
		.desc = "Encrypt PEM output with CBC Camellia",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_cipher,
	},
#endif
#ifndef OPENSSL_NO_DES
	{
		.name = "des",
		.desc = "Encrypt with DES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_cipher,
	},
	{
		.name = "des3",
		.desc = "Encrypt with triple DES",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_cipher,
	},
#endif
#ifndef OPENSSL_NO_RC2
	{
		.name = "rc2-40",
		.desc = "Encrypt with RC2-40 (default)",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_cipher,
	},
	{
		.name = "rc2-64",
		.desc = "Encrypt with RC2-64",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_cipher,
	},
	{
		.name = "rc2-128",
		.desc = "Encrypt with RC2-128",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_cipher,
	},
#endif
	{
		.name = "CAfile",
		.argname = "file",
		.desc = "Certificate Authority file",
		.type = OPTION_ARG,
		.opt.arg = &smime_config.CAfile,
	},
	{
		.name = "CApath",
		.argname = "path",
		.desc = "Certificate Authority path",
		.type = OPTION_ARG,
		.opt.arg = &smime_config.CApath,
	},
	{
		.name = "binary",
		.desc = "Do not translate message to text",
		.type = OPTION_VALUE_OR,
		.opt.value = &smime_config.flags,
		.value = PKCS7_BINARY,
	},
	{
		.name = "certfile",
		.argname = "file",
		.desc = "Other certificates file",
		.type = OPTION_ARG,
		.opt.arg = &smime_config.certfile,
	},
	{
		.name = "content",
		.argname = "file",
		.desc = "Supply or override content for detached signature",
		.type = OPTION_ARG,
		.opt.arg = &smime_config.contfile,
	},
	{
		.name = "crlfeol",
		.desc = "Use CRLF as EOL termination instead of CR only",
		.type = OPTION_VALUE_OR,
		.opt.value = &smime_config.flags,
		.value = PKCS7_CRLFEOL,
	},
	{
		.name = "decrypt",
		.desc = "Decrypt encrypted message",
		.type = OPTION_VALUE,
		.opt.value = &smime_config.operation,
		.value = SMIME_DECRYPT,
	},
	{
		.name = "encrypt",
		.desc = "Encrypt message",
		.type = OPTION_VALUE,
		.opt.value = &smime_config.operation,
		.value = SMIME_ENCRYPT,
	},
	{
		.name = "from",
		.argname = "addr",
		.desc = "From address",
		.type = OPTION_ARG,
		.opt.arg = &smime_config.from,
	},
	{
		.name = "in",
		.argname = "file",
		.desc = "Input file",
		.type = OPTION_ARG,
		.opt.arg = &smime_config.infile,
	},
	{
		.name = "indef",
		.desc = "Same as -stream",
		.type = OPTION_VALUE,
		.opt.value = &smime_config.indef,
		.value = 1,
	},
	{
		.name = "inform",
		.argname = "fmt",
		.desc = "Input format (DER, PEM or SMIME (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &smime_config.informat,
	},
	{
		.name = "inkey",
		.argname = "file",
		.desc = "Input key file",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = smime_opt_inkey,
	},
	{
		.name = "keyform",
		.argname = "fmt",
		.desc = "Input key format (DER or PEM (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &smime_config.keyform,
	},
	{
		.name = "md",
		.argname = "digest",
		.desc = "Digest to use when signing or resigning",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = smime_opt_md,
	},
	{
		.name = "noattr",
		.desc = "Do not include any signed attributes",
		.type = OPTION_VALUE_OR,
		.opt.value = &smime_config.flags,
		.value = PKCS7_NOATTR,
	},
	{
		.name = "nocerts",
		.desc = "Do not include signer's certificate when signing",
		.type = OPTION_VALUE_OR,
		.opt.value = &smime_config.flags,
		.value = PKCS7_NOCERTS,
	},
	{
		.name = "nochain",
		.desc = "Do not chain verification of signer's certificates",
		.type = OPTION_VALUE_OR,
		.opt.value = &smime_config.flags,
		.value = PKCS7_NOCHAIN,
	},
	{
		.name = "nodetach",
		.desc = "Use opaque signing",
		.type = OPTION_VALUE_AND,
		.opt.value = &smime_config.flags,
		.value = ~PKCS7_DETACHED,
	},
	{
		.name = "noindef",
		.desc = "Disable streaming I/O",
		.type = OPTION_VALUE,
		.opt.value = &smime_config.indef,
		.value = 0,
	},
	{
		.name = "nointern",
		.desc = "Do not search certificates in message for signer",
		.type = OPTION_VALUE_OR,
		.opt.value = &smime_config.flags,
		.value = PKCS7_NOINTERN,
	},
	{
		.name = "nooldmime",
		.desc = "Output old S/MIME content type",
		.type = OPTION_VALUE_OR,
		.opt.value = &smime_config.flags,
		.value = PKCS7_NOOLDMIMETYPE,
	},
	{
		.name = "nosigs",
		.desc = "Do not verify message signature",
		.type = OPTION_VALUE_OR,
		.opt.value = &smime_config.flags,
		.value = PKCS7_NOSIGS,
	},
	{
		.name = "nosmimecap",
		.desc = "Omit the SMIMECapabilities attribute",
		.type = OPTION_VALUE_OR,
		.opt.value = &smime_config.flags,
		.value = PKCS7_NOSMIMECAP,
	},
	{
		.name = "noverify",
		.desc = "Do not verify signer's certificate",
		.type = OPTION_VALUE_OR,
		.opt.value = &smime_config.flags,
		.value = PKCS7_NOVERIFY,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output file",
		.type = OPTION_ARG,
		.opt.arg = &smime_config.outfile,
	},
	{
		.name = "outform",
		.argname = "fmt",
		.desc = "Output format (DER, PEM or SMIME (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &smime_config.outformat,
	},
	{
		.name = "passin",
		.argname = "src",
		.desc = "Private key password source",
		.type = OPTION_ARG,
		.opt.arg = &smime_config.passargin,
	},
	{
		.name = "pk7out",
		.desc = "Output PKCS#7 structure",
		.type = OPTION_VALUE,
		.opt.value = &smime_config.operation,
		.value = SMIME_PK7OUT,
	},
	{
		.name = "recip",
		.argname = "file",
		.desc = "Recipient certificate file for decryption",
		.type = OPTION_ARG,
		.opt.arg = &smime_config.recipfile,
	},
	{
		.name = "resign",
		.desc = "Resign a signed message",
		.type = OPTION_VALUE,
		.opt.value = &smime_config.operation,
		.value = SMIME_RESIGN,
	},
	{
		.name = "sign",
		.desc = "Sign message",
		.type = OPTION_VALUE,
		.opt.value = &smime_config.operation,
		.value = SMIME_SIGN,
	},
	{
		.name = "signer",
		.argname = "file",
		.desc = "Signer certificate file",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = smime_opt_signer,
	},
	{
		.name = "stream",
		.desc = "Enable streaming I/O",
		.type = OPTION_VALUE,
		.opt.value = &smime_config.indef,
		.value = 1,
	},
	{
		.name = "subject",
		.argname = "s",
		.desc = "Subject",
		.type = OPTION_ARG,
		.opt.arg = &smime_config.subject,
	},
	{
		.name = "text",
		.desc = "Include or delete text MIME headers",
		.type = OPTION_VALUE_OR,
		.opt.value = &smime_config.flags,
		.value = PKCS7_TEXT,
	},
	{
		.name = "to",
		.argname = "addr",
		.desc = "To address",
		.type = OPTION_ARG,
		.opt.arg = &smime_config.to,
	},
	{
		.name = "verify",
		.desc = "Verify signed message",
		.type = OPTION_VALUE,
		.opt.value = &smime_config.operation,
		.value = SMIME_VERIFY,
	},
	{
		.name = "check_ss_sig",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_verify_param,
	},
	{
		.name = "crl_check",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_verify_param,
	},
	{
		.name = "crl_check_all",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_verify_param,
	},
	{
		.name = "extended_crl",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_verify_param,
	},
	{
		.name = "ignore_critical",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_verify_param,
	},
	{
		.name = "issuer_checks",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_verify_param,
	},
	{
		.name = "policy_check",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_verify_param,
	},
	{
		.name = "x509_strict",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_verify_param,
	},
	{
		.name = NULL,
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = smime_opt_cipher,
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
		.name = "policy_check",
		.desc = "Enable certificate policy checking",
	},
	{
		.name = "x509_strict",
		.desc = "Use strict X.509 rules (disables workarounds)",
	},
	{ NULL },
};

static void
smime_usage(void)
{
	fprintf(stderr, "usage: smime "
	    "[-aes128 | -aes192 | -aes256 | -des |\n"
	    "    -des3 | -rc2-40 | -rc2-64 | -rc2-128] [-binary]\n"
	    "    [-CAfile file] [-CApath directory] [-certfile file]\n"
	    "    [-content file]\n"
	    "    [-decrypt] [-encrypt]\n"
	    "    [-from addr] [-in file] [-indef]\n"
	    "    [-inform der | pem | smime] [-inkey file]\n"
	    "    [-keyform der | pem] [-md digest] [-noattr] [-nocerts]\n"
	    "    [-nochain] [-nodetach] [-noindef] [-nointern] [-nosigs]\n"
	    "    [-nosmimecap] [-noverify] [-out file]\n"
	    "    [-outform der | pem | smime] [-passin arg] [-pk7out]\n"
	    "    [-recip file] [-resign] [-sign]\n"
	    "    [-signer file] [-stream] [-subject s] [-text] [-to addr]\n"
	    "    [-verify] [cert.pem ...]\n\n");

	options_usage(smime_options);

	fprintf(stderr, "\nVerification options:\n\n");
	options_usage(verify_shared_options);
}

int
smime_main(int argc, char **argv)
{
	int ret = 0;
	char **args;
	int argsused = 0;
	const char *inmode = "r", *outmode = "w";
	PKCS7 *p7 = NULL;
	X509_STORE *store = NULL;
	X509 *cert = NULL, *recip = NULL, *signer = NULL;
	EVP_PKEY *key = NULL;
	STACK_OF(X509) *encerts = NULL, *other = NULL;
	BIO *in = NULL, *out = NULL, *indata = NULL;
	int badarg = 0;
	char *passin = NULL;

	if (single_execution) {
		if (pledge("stdio cpath wpath rpath tty", NULL) == -1) {
			perror("pledge");
			exit(1);
		}
	}

	memset(&smime_config, 0, sizeof(smime_config));
	smime_config.flags = PKCS7_DETACHED;
	smime_config.informat = FORMAT_SMIME;
	smime_config.outformat = FORMAT_SMIME;
	smime_config.keyform = FORMAT_PEM;
	if (options_parse(argc, argv, smime_options, NULL, &argsused) != 0) {
		goto argerr;
	}
	args = argv + argsused;
	ret = 1;

	if (!(smime_config.operation & SMIME_SIGNERS) &&
	    (smime_config.skkeys != NULL || smime_config.sksigners != NULL)) {
		BIO_puts(bio_err, "Multiple signers or keys not allowed\n");
		goto argerr;
	}
	if (smime_config.operation & SMIME_SIGNERS) {
		/* Check to see if any final signer needs to be appended */
		if (smime_config.keyfile != NULL &&
		    smime_config.signerfile == NULL) {
			BIO_puts(bio_err, "Illegal -inkey without -signer\n");
			goto argerr;
		}
		if (smime_config.signerfile != NULL) {
			if (smime_config.sksigners == NULL) {
				if ((smime_config.sksigners =
				    sk_OPENSSL_STRING_new_null()) == NULL)
					goto end;
			}
			if (!sk_OPENSSL_STRING_push(smime_config.sksigners,
			    smime_config.signerfile))
				goto end;
			if (smime_config.skkeys == NULL) {
				if ((smime_config.skkeys =
				    sk_OPENSSL_STRING_new_null()) == NULL)
					goto end;
			}
			if (smime_config.keyfile == NULL)
				smime_config.keyfile = smime_config.signerfile;
			if (!sk_OPENSSL_STRING_push(smime_config.skkeys,
			    smime_config.keyfile))
				goto end;
		}
		if (smime_config.sksigners == NULL) {
			BIO_printf(bio_err,
			    "No signer certificate specified\n");
			badarg = 1;
		}
		smime_config.signerfile = NULL;
		smime_config.keyfile = NULL;
	} else if (smime_config.operation == SMIME_DECRYPT) {
		if (smime_config.recipfile == NULL &&
		    smime_config.keyfile == NULL) {
			BIO_printf(bio_err,
			    "No recipient certificate or key specified\n");
			badarg = 1;
		}
	} else if (smime_config.operation == SMIME_ENCRYPT) {
		if (*args == NULL) {
			BIO_printf(bio_err,
			    "No recipient(s) certificate(s) specified\n");
			badarg = 1;
		}
	} else if (!smime_config.operation) {
		badarg = 1;
	}

	if (badarg) {
 argerr:
		smime_usage();
		goto end;
	}

	if (!app_passwd(bio_err, smime_config.passargin, NULL, &passin, NULL)) {
		BIO_printf(bio_err, "Error getting password\n");
		goto end;
	}
	ret = 2;

	if (!(smime_config.operation & SMIME_SIGNERS))
		smime_config.flags &= ~PKCS7_DETACHED;

	if (smime_config.operation & SMIME_OP) {
		if (smime_config.outformat == FORMAT_ASN1)
			outmode = "wb";
	} else {
		if (smime_config.flags & PKCS7_BINARY)
			outmode = "wb";
	}

	if (smime_config.operation & SMIME_IP) {
		if (smime_config.informat == FORMAT_ASN1)
			inmode = "rb";
	} else {
		if (smime_config.flags & PKCS7_BINARY)
			inmode = "rb";
	}

	if (smime_config.operation == SMIME_ENCRYPT) {
		if (smime_config.cipher == NULL) {
#ifndef OPENSSL_NO_RC2
			smime_config.cipher = EVP_rc2_40_cbc();
#else
			BIO_printf(bio_err, "No cipher selected\n");
			goto end;
#endif
		}
		if ((encerts = sk_X509_new_null()) == NULL)
			goto end;
		while (*args != NULL) {
			if ((cert = load_cert(bio_err, *args, FORMAT_PEM,
			    NULL, "recipient certificate file")) == NULL) {
				goto end;
			}
			if (!sk_X509_push(encerts, cert))
				goto end;
			cert = NULL;
			args++;
		}
	}
	if (smime_config.certfile != NULL) {
		if ((other = load_certs(bio_err, smime_config.certfile,
		    FORMAT_PEM, NULL, "certificate file")) == NULL) {
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	if (smime_config.recipfile != NULL &&
	    (smime_config.operation == SMIME_DECRYPT)) {
		if ((recip = load_cert(bio_err, smime_config.recipfile,
		    FORMAT_PEM, NULL, "recipient certificate file")) == NULL) {
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	if (smime_config.operation == SMIME_DECRYPT) {
		if (smime_config.keyfile == NULL)
			smime_config.keyfile = smime_config.recipfile;
	} else if (smime_config.operation == SMIME_SIGN) {
		if (smime_config.keyfile == NULL)
			smime_config.keyfile = smime_config.signerfile;
	} else {
		smime_config.keyfile = NULL;
	}

	if (smime_config.keyfile != NULL) {
		key = load_key(bio_err, smime_config.keyfile,
		    smime_config.keyform, 0, passin, "signing key file");
		if (key == NULL)
			goto end;
	}
	if (smime_config.infile != NULL) {
		if ((in = BIO_new_file(smime_config.infile, inmode)) == NULL) {
			BIO_printf(bio_err,
			    "Can't open input file %s\n", smime_config.infile);
			goto end;
		}
	} else {
		if ((in = BIO_new_fp(stdin, BIO_NOCLOSE)) == NULL)
			goto end;
	}

	if (smime_config.operation & SMIME_IP) {
		if (smime_config.informat == FORMAT_SMIME)
			p7 = SMIME_read_PKCS7(in, &indata);
		else if (smime_config.informat == FORMAT_PEM)
			p7 = PEM_read_bio_PKCS7(in, NULL, NULL, NULL);
		else if (smime_config.informat == FORMAT_ASN1)
			p7 = d2i_PKCS7_bio(in, NULL);
		else {
			BIO_printf(bio_err,
			    "Bad input format for PKCS#7 file\n");
			goto end;
		}

		if (p7 == NULL) {
			BIO_printf(bio_err, "Error reading S/MIME message\n");
			goto end;
		}
		if (smime_config.contfile != NULL) {
			BIO_free(indata);
			if ((indata = BIO_new_file(smime_config.contfile,
			    "rb")) == NULL) {
				BIO_printf(bio_err,
				    "Can't read content file %s\n",
				    smime_config.contfile);
				goto end;
			}
		}
	}
	if (smime_config.outfile != NULL) {
		if ((out = BIO_new_file(smime_config.outfile, outmode)) == NULL) {
			BIO_printf(bio_err,
			    "Can't open output file %s\n",
			    smime_config.outfile);
			goto end;
		}
	} else {
		if ((out = BIO_new_fp(stdout, BIO_NOCLOSE)) == NULL)
			goto end;
	}

	if (smime_config.operation == SMIME_VERIFY) {
		if ((store = setup_verify(bio_err, smime_config.CAfile,
		    smime_config.CApath)) == NULL)
			goto end;
		X509_STORE_set_verify_cb(store, smime_cb);
		if (smime_config.vpm != NULL) {
			if (!X509_STORE_set1_param(store, smime_config.vpm))
				goto end;
		}
	}
	ret = 3;

	if (smime_config.operation == SMIME_ENCRYPT) {
		if (smime_config.indef)
			smime_config.flags |= PKCS7_STREAM;
		p7 = PKCS7_encrypt(encerts, in, smime_config.cipher,
		    smime_config.flags);
	} else if (smime_config.operation & SMIME_SIGNERS) {
		int i;
		/*
		 * If detached data content we only enable streaming if
		 * S/MIME output format.
		 */
		if (smime_config.operation == SMIME_SIGN) {
			if (smime_config.flags & PKCS7_DETACHED) {
				if (smime_config.outformat == FORMAT_SMIME)
					smime_config.flags |= PKCS7_STREAM;
			} else if (smime_config.indef) {
				smime_config.flags |= PKCS7_STREAM;
			}
			smime_config.flags |= PKCS7_PARTIAL;
			p7 = PKCS7_sign(NULL, NULL, other, in,
			    smime_config.flags);
			if (p7 == NULL)
				goto end;
		} else {
			smime_config.flags |= PKCS7_REUSE_DIGEST;
		}
		for (i = 0; i < sk_OPENSSL_STRING_num(smime_config.sksigners); i++) {
			smime_config.signerfile =
			    sk_OPENSSL_STRING_value(smime_config.sksigners, i);
			smime_config.keyfile =
			    sk_OPENSSL_STRING_value(smime_config.skkeys, i);
			signer = load_cert(bio_err, smime_config.signerfile,
			    FORMAT_PEM, NULL, "signer certificate");
			if (signer == NULL)
				goto end;
			key = load_key(bio_err, smime_config.keyfile,
			    smime_config.keyform, 0, passin,
			    "signing key file");
			if (key == NULL)
				goto end;
			if (PKCS7_sign_add_signer(p7, signer, key,
			    smime_config.sign_md, smime_config.flags) == NULL)
				goto end;
			X509_free(signer);
			signer = NULL;
			EVP_PKEY_free(key);
			key = NULL;
		}
		/* If not streaming or resigning finalize structure */
		if ((smime_config.operation == SMIME_SIGN) &&
		    !(smime_config.flags & PKCS7_STREAM)) {
			if (!PKCS7_final(p7, in, smime_config.flags))
				goto end;
		}
	}
	if (p7 == NULL) {
		BIO_printf(bio_err, "Error creating PKCS#7 structure\n");
		goto end;
	}
	ret = 4;

	if (smime_config.operation == SMIME_DECRYPT) {
		if (!PKCS7_decrypt(p7, key, recip, out, smime_config.flags)) {
			BIO_printf(bio_err,
			    "Error decrypting PKCS#7 structure\n");
			goto end;
		}
	} else if (smime_config.operation == SMIME_VERIFY) {
		STACK_OF(X509) *signers;
		if (PKCS7_verify(p7, other, store, indata, out,
		    smime_config.flags)) {
			BIO_printf(bio_err, "Verification successful\n");
		} else {
			BIO_printf(bio_err, "Verification failure\n");
			goto end;
		}
		if ((signers = PKCS7_get0_signers(p7, other,
		    smime_config.flags)) == NULL)
			goto end;
		if (!save_certs(smime_config.signerfile, signers)) {
			BIO_printf(bio_err, "Error writing signers to %s\n",
			    smime_config.signerfile);
			sk_X509_free(signers);
			ret = 5;
			goto end;
		}
		sk_X509_free(signers);
	} else if (smime_config.operation == SMIME_PK7OUT) {
		PEM_write_bio_PKCS7(out, p7);
	} else {
		if (smime_config.to != NULL)
			BIO_printf(out, "To: %s\n", smime_config.to);
		if (smime_config.from != NULL)
			BIO_printf(out, "From: %s\n", smime_config.from);
		if (smime_config.subject != NULL)
			BIO_printf(out, "Subject: %s\n", smime_config.subject);
		if (smime_config.outformat == FORMAT_SMIME) {
			if (smime_config.operation == SMIME_RESIGN) {
				if (!SMIME_write_PKCS7(out, p7, indata,
				    smime_config.flags))
					goto end;
			} else {
				if (!SMIME_write_PKCS7(out, p7, in,
				    smime_config.flags))
					goto end;
			}
		} else if (smime_config.outformat == FORMAT_PEM) {
			if (!PEM_write_bio_PKCS7_stream(out, p7, in,
			    smime_config.flags))
				goto end;
		} else if (smime_config.outformat == FORMAT_ASN1) {
			if (!i2d_PKCS7_bio_stream(out, p7, in,
			    smime_config.flags))
				goto end;
		} else {
			BIO_printf(bio_err,
			    "Bad output format for PKCS#7 file\n");
			goto end;
		}
	}

	ret = 0;

 end:
	if (ret)
		ERR_print_errors(bio_err);
	sk_X509_pop_free(encerts, X509_free);
	sk_X509_pop_free(other, X509_free);
	X509_VERIFY_PARAM_free(smime_config.vpm);
	sk_OPENSSL_STRING_free(smime_config.sksigners);
	sk_OPENSSL_STRING_free(smime_config.skkeys);
	X509_STORE_free(store);
	X509_free(cert);
	X509_free(recip);
	X509_free(signer);
	EVP_PKEY_free(key);
	PKCS7_free(p7);
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

/* Minimal callback just to output policy info (if any) */
static int
smime_cb(int ok, X509_STORE_CTX *ctx)
{
	int error;

	error = X509_STORE_CTX_get_error(ctx);

	if ((error != X509_V_ERR_NO_EXPLICIT_POLICY) &&
	    ((error != X509_V_OK) || (ok != 2)))
		return ok;

	policies_print(NULL, ctx);

	return ok;
}
