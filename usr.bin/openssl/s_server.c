/* $OpenBSD: s_server.c,v 1.58 2023/07/03 08:03:56 beck Exp $ */
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
/* ====================================================================
 * Copyright (c) 1998-2006 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECC cipher suite support in OpenSSL originally developed by
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */
/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE.
 */

/* Until the key-gen callbacks are modified to use newer prototypes, we allow
 * deprecated functions for openssl-internal code */
#ifdef OPENSSL_NO_DEPRECATED
#undef OPENSSL_NO_DEPRECATED
#endif

#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include "apps.h"

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/lhash.h>
#include <openssl/ocsp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#ifndef OPENSSL_NO_DH
#include <openssl/dh.h>
#endif

#include <openssl/rsa.h>

#include "s_apps.h"
#include "timeouts.h"

static void s_server_init(void);
static void sv_usage(void);
static void print_stats(BIO *bp, SSL_CTX *ctx);
static int sv_body(int s, unsigned char *context);
static void close_accept_socket(void);
static int init_ssl_connection(SSL *s);
#ifndef OPENSSL_NO_DH
static DH *load_dh_param(const char *dhfile);
#endif
static int www_body(int s, unsigned char *context);
static int generate_session_id(const SSL *ssl, unsigned char *id,
    unsigned int *id_len);
static int ssl_servername_cb(SSL *s, int *ad, void *arg);
static int cert_status_cb(SSL * s, void *arg);
static int alpn_cb(SSL *s, const unsigned char **out, unsigned char *outlen,
    const unsigned char *in, unsigned int inlen, void *arg);
/* static int load_CA(SSL_CTX *ctx, char *file);*/

#define BUFSIZZ	16*1024
static int bufsize = BUFSIZZ;
static int accept_socket = -1;

#define TEST_CERT	"server.pem"
#define TEST_CERT2	"server2.pem"

static int s_server_session_id_context = 1;	/* anything will do */
static SSL_CTX *ctx = NULL;
static SSL_CTX *ctx2 = NULL;
static BIO *bio_s_out = NULL;

static int local_argc = 0;
static char **local_argv;

/* This is a context that we pass to callbacks */
typedef struct tlsextctx_st {
	char *servername;
	BIO *biodebug;
	int extension_error;
} tlsextctx;

/* Structure passed to cert status callback */
typedef struct tlsextstatusctx_st {
	/* Default responder to use */
	char *host, *path, *port;
	int use_ssl;
	int timeout;
	BIO *err;
	int verbose;
} tlsextstatusctx;

/* This the context that we pass to alpn_cb */
typedef struct tlsextalpnctx_st {
	unsigned char *data;
	unsigned short len;
} tlsextalpnctx;

static struct {
	char *alpn_in;
	char *npn_in; /* Ignored. */
	int bugs;
	char *CAfile;
	char *CApath;
#ifndef OPENSSL_NO_DTLS
	int cert_chain;
#endif
	char *cert_file;
	char *cert_file2;
	int cert_format;
	char *cipher;
	unsigned char *context;
	int crlf;
	char *dcert_file;
	int dcert_format;
	int debug;
	char *dhfile;
	char *dkey_file;
	int dkey_format;
	char *dpassarg;
	int enable_timeouts;
	const char *errstr;
	char *groups_in;
	char *key_file;
	char *key_file2;
	int key_format;
	char *keymatexportlabel;
	int keymatexportlen;
	uint16_t max_version;
	uint16_t min_version;
	const SSL_METHOD *meth;
	int msg;
	int naccept;
	char *named_curve;
	int nbio;
	int nbio_test;
	int no_cache;
	int nocert;
	int no_dhe;
	int no_ecdhe;
	int no_tmp_rsa; /* No-op. */
	int off;
	char *passarg;
	short port;
	int quiet;
	int server_verify;
	char *session_id_prefix;
	long socket_mtu;
	int socket_type;
#ifndef OPENSSL_NO_SRTP
	char *srtp_profiles;
#endif
	int state;
	tlsextstatusctx tlscstatp;
	tlsextctx tlsextcbp;
	int tlsextdebug;
	int tlsextstatus;
	X509_VERIFY_PARAM *vpm;
	int www;
} cfg;

static int
s_server_opt_context(char *arg)
{
	cfg.context = (unsigned char *) arg;
	return (0);
}

static int
s_server_opt_keymatexportlen(char *arg)
{
	cfg.keymatexportlen = strtonum(arg, 1, INT_MAX,
	    &cfg.errstr);
	if (cfg.errstr != NULL) {
		BIO_printf(bio_err, "invalid argument %s: %s\n",
		    arg, cfg.errstr);
		return (1);
	}
	return (0);
}

#ifndef OPENSSL_NO_DTLS
static int
s_server_opt_mtu(char *arg)
{
	cfg.socket_mtu = strtonum(arg, 0, LONG_MAX,
	    &cfg.errstr);
	if (cfg.errstr != NULL) {
		BIO_printf(bio_err, "invalid argument %s: %s\n",
		    arg, cfg.errstr);
		return (1);
	}
	return (0);
}
#endif

#ifndef OPENSSL_NO_DTLS
static int
s_server_opt_protocol_version_dtls(void)
{
	cfg.meth = DTLS_server_method();
	cfg.socket_type = SOCK_DGRAM;
	return (0);
}
#endif

#ifndef OPENSSL_NO_DTLS1_2
static int
s_server_opt_protocol_version_dtls1_2(void)
{
	cfg.meth = DTLS_server_method();
	cfg.min_version = DTLS1_2_VERSION;
	cfg.max_version = DTLS1_2_VERSION;
	cfg.socket_type = SOCK_DGRAM;
	return (0);
}
#endif

static int
s_server_opt_protocol_version_tls1_2(void)
{
	cfg.min_version = TLS1_2_VERSION;
	cfg.max_version = TLS1_2_VERSION;
	return (0);
}

static int
s_server_opt_protocol_version_tls1_3(void)
{
	cfg.min_version = TLS1_3_VERSION;
	cfg.max_version = TLS1_3_VERSION;
	return (0);
}

static int
s_server_opt_nbio_test(void)
{
	cfg.nbio = 1;
	cfg.nbio_test = 1;
	return (0);
}

static int
s_server_opt_port(char *arg)
{
	if (!extract_port(arg, &cfg.port))
		return (1);
	return (0);
}

static int
s_server_opt_status_timeout(char *arg)
{
	cfg.tlsextstatus = 1;
	cfg.tlscstatp.timeout = strtonum(arg, 0, INT_MAX,
	    &cfg.errstr);
	if (cfg.errstr != NULL) {
		BIO_printf(bio_err, "invalid argument %s: %s\n",
		    arg, cfg.errstr);
		return (1);
	}
	return (0);
}

static int
s_server_opt_status_url(char *arg)
{
	cfg.tlsextstatus = 1;
	if (!OCSP_parse_url(arg, &cfg.tlscstatp.host,
	    &cfg.tlscstatp.port, &cfg.tlscstatp.path,
	    &cfg.tlscstatp.use_ssl)) {
		BIO_printf(bio_err, "Error parsing URL\n");
		return (1);
	}
	return (0);
}

static int
s_server_opt_status_verbose(void)
{
	cfg.tlsextstatus = 1;
	cfg.tlscstatp.verbose = 1;
	return (0);
}

static int
s_server_opt_verify(char *arg)
{
	cfg.server_verify = SSL_VERIFY_PEER |
	    SSL_VERIFY_CLIENT_ONCE;
	verify_depth = strtonum(arg, 0, INT_MAX, &cfg.errstr);
	if (cfg.errstr != NULL) {
		BIO_printf(bio_err, "invalid argument %s: %s\n",
		    arg, cfg.errstr);
		return (1);
	}
	BIO_printf(bio_err, "verify depth is %d\n", verify_depth);
	return (0);
}

static int
s_server_opt_verify_fail(char *arg)
{
	cfg.server_verify = SSL_VERIFY_PEER |
	    SSL_VERIFY_FAIL_IF_NO_PEER_CERT | SSL_VERIFY_CLIENT_ONCE;
	verify_depth = strtonum(arg, 0, INT_MAX, &cfg.errstr);
	if (cfg.errstr != NULL) {
		BIO_printf(bio_err, "invalid argument %s: %s\n",
		    arg, cfg.errstr);
		return (1);
	}
	BIO_printf(bio_err, "verify depth is %d, must return a certificate\n",
	    verify_depth);
	return (0);
}

static int
s_server_opt_verify_param(int argc, char **argv, int *argsused)
{
	char **pargs = argv;
	int pargc = argc;
	int badarg = 0;

	if (!args_verify(&pargs, &pargc, &badarg, bio_err,
	    &cfg.vpm)) {
		BIO_printf(bio_err, "unknown option %s\n", *argv);
		return (1);
	}
	if (badarg)
		return (1);

	*argsused = argc - pargc;
	return (0);
}

static const struct option s_server_options[] = {
	{
		.name = "4",
		.type = OPTION_DISCARD,
	},
	{
		.name = "6",
		.type = OPTION_DISCARD,
	},
	{
		.name = "accept",
		.argname = "port",
		.desc = "Port to accept on (default is 4433)",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = s_server_opt_port,
	},
	{
		.name = "alpn",
		.argname = "protocols",
		.desc = "Set the advertised protocols for the ALPN extension"
			" (comma-separated list)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.alpn_in,
	},
	{
		.name = "bugs",
		.desc = "Turn on SSL bug compatibility",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.bugs,
	},
	{
		.name = "CAfile",
		.argname = "file",
		.desc = "PEM format file of CA certificates",
		.type = OPTION_ARG,
		.opt.arg = &cfg.CAfile,
	},
	{
		.name = "CApath",
		.argname = "directory",
		.desc = "PEM format directory of CA certificates",
		.type = OPTION_ARG,
		.opt.arg = &cfg.CApath,
	},
	{
		.name = "cert",
		.argname = "file",
		.desc = "Certificate file to use\n"
			"(default is " TEST_CERT ")",
		.type = OPTION_ARG,
		.opt.arg = &cfg.cert_file,
	},
	{
		.name = "cert2",
		.argname = "file",
		.desc = "Certificate file to use for servername\n"
			"(default is " TEST_CERT2 ")",
		.type = OPTION_ARG,
		.opt.arg = &cfg.cert_file2,
	},
	{
		.name = "certform",
		.argname = "fmt",
		.desc = "Certificate format (PEM or DER) PEM default",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.cert_format,
	},
#ifndef OPENSSL_NO_DTLS
	{
		.name = "chain",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.cert_chain,
	},
#endif
	{
		.name = "cipher",
		.argname = "list",
		.desc = "List of ciphers to enable (see `openssl ciphers`)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.cipher,
	},
	{
		.name = "context",
		.argname = "id",
		.desc = "Set session ID context",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = s_server_opt_context,
	},
	{
		.name = "crlf",
		.desc = "Convert LF from terminal into CRLF",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.crlf,
	},
	{
		.name = "dcert",
		.argname = "file",
		.desc = "Second certificate file to use (usually for DSA)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.dcert_file,
	},
	{
		.name = "dcertform",
		.argname = "fmt",
		.desc = "Second certificate format (PEM or DER) PEM default",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.dcert_format,
	},
	{
		.name = "debug",
		.desc = "Print more output",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.debug,
	},
	{
		.name = "dhparam",
		.argname = "file",
		.desc = "DH parameter file to use, in cert file if not specified",
		.type = OPTION_ARG,
		.opt.arg = &cfg.dhfile,
	},
	{
		.name = "dkey",
		.argname = "file",
		.desc = "Second private key file to use (usually for DSA)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.dkey_file,
	},
	{
		.name = "dkeyform",
		.argname = "fmt",
		.desc = "Second key format (PEM or DER) PEM default",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.dkey_format,
	},
	{
		.name = "dpass",
		.argname = "arg",
		.desc = "Second private key file pass phrase source",
		.type = OPTION_ARG,
		.opt.arg = &cfg.dpassarg,
	},
#ifndef OPENSSL_NO_DTLS
	{
		.name = "dtls",
		.desc = "Use any version of DTLS",
		.type = OPTION_FUNC,
		.opt.func = s_server_opt_protocol_version_dtls,
	},
#endif
#ifndef OPENSSL_NO_DTLS1_2
	{
		.name = "dtls1_2",
		.desc = "Just use DTLSv1.2",
		.type = OPTION_FUNC,
		.opt.func = s_server_opt_protocol_version_dtls1_2,
	},
#endif
	{
		.name = "groups",
		.argname = "list",
		.desc = "Specify EC groups (colon-separated list)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.groups_in,
	},
	{
		.name = "HTTP",
		.desc = "Respond to a 'GET /<path> HTTP/1.0' with file ./<path>",
		.type = OPTION_VALUE,
		.opt.value = &cfg.www,
		.value = 3,
	},
	{
		.name = "id_prefix",
		.argname = "arg",
		.desc = "Generate SSL/TLS session IDs prefixed by 'arg'",
		.type = OPTION_ARG,
		.opt.arg = &cfg.session_id_prefix,
	},
	{
		.name = "key",
		.argname = "file",
		.desc = "Private Key file to use, in cert file if\n"
			"not specified (default is " TEST_CERT ")",
		.type = OPTION_ARG,
		.opt.arg = &cfg.key_file,
	},
	{
		.name = "key2",
		.argname = "file",
		.desc = "Private Key file to use for servername, in cert file if\n"
			"not specified (default is " TEST_CERT2 ")",
		.type = OPTION_ARG,
		.opt.arg = &cfg.key_file2,
	},
	{
		.name = "keyform",
		.argname = "fmt",
		.desc = "Key format (PEM or DER) PEM default",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.key_format,
	},
	{
		.name = "keymatexport",
		.argname = "label",
		.desc = "Export keying material using label",
		.type = OPTION_ARG,
		.opt.arg = &cfg.keymatexportlabel,
	},
	{
		.name = "keymatexportlen",
		.argname = "len",
		.desc = "Export len bytes of keying material (default 20)",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = s_server_opt_keymatexportlen,
	},
	{
		.name = "legacy_renegotiation",
		.type = OPTION_DISCARD,
	},
	{
		.name = "msg",
		.desc = "Show protocol messages",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.msg,
	},
#ifndef OPENSSL_NO_DTLS
	{
		.name = "mtu",
		.argname = "mtu",
		.desc = "Set link layer MTU",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = s_server_opt_mtu,
	},
#endif
	{
		.name = "naccept",
		.argname = "num",
		.desc = "Terminate after num connections",
		.type = OPTION_ARG_INT,
		.opt.value = &cfg.naccept
	},
	{
		.name = "named_curve",
		.argname = "arg",
		.type = OPTION_ARG,
		.opt.arg = &cfg.named_curve,
	},
	{
		.name = "nbio",
		.desc = "Run with non-blocking I/O",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.nbio,
	},
	{
		.name = "nbio_test",
		.desc = "Test with the non-blocking test bio",
		.type = OPTION_FUNC,
		.opt.func = s_server_opt_nbio_test,
	},
	{
		.name = "nextprotoneg",
		.argname = "arg",
		.type = OPTION_ARG,
		.opt.arg = &cfg.npn_in, /* Ignored. */
	},
	{
		.name = "no_cache",
		.desc = "Disable session cache",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.no_cache,
	},
	{
		.name = "no_comp",
		.desc = "Disable SSL/TLS compression",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.off,
		.value = SSL_OP_NO_COMPRESSION,
	},
	{
		.name = "no_dhe",
		.desc = "Disable ephemeral DH",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.no_dhe,
	},
	{
		.name = "no_ecdhe",
		.desc = "Disable ephemeral ECDH",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.no_ecdhe,
	},
	{
		.name = "no_ticket",
		.desc = "Disable use of RFC4507bis session tickets",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.off,
		.value = SSL_OP_NO_TICKET,
	},
	{
		.name = "no_ssl2",
		.type = OPTION_DISCARD,
	},
	{
		.name = "no_ssl3",
		.type = OPTION_DISCARD,
	},
	{
		.name = "no_tls1",
		.type = OPTION_DISCARD,
	},
	{
		.name = "no_tls1_1",
		.type = OPTION_DISCARD,
	},
	{
		.name = "no_tls1_2",
		.desc = "Just disable TLSv1.2",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.off,
		.value = SSL_OP_NO_TLSv1_2,
	},
	{
		.name = "no_tls1_3",
		.desc = "Just disable TLSv1.3",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.off,
		.value = SSL_OP_NO_TLSv1_3,
	},
	{
		.name = "no_tmp_rsa",
		.type = OPTION_DISCARD,
	},
	{
		.name = "nocert",
		.desc = "Don't use any certificates (Anon-DH)",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.nocert,
	},
	{
		.name = "pass",
		.argname = "arg",
		.desc = "Private key file pass phrase source",
		.type = OPTION_ARG,
		.opt.arg = &cfg.passarg,
	},
	{
		.name = "port",
		.argname = "port",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = s_server_opt_port,
	},
	{
		.name = "quiet",
		.desc = "Inhibit printing of session and certificate information",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.quiet,
	},
	{
		.name = "servername",
		.argname = "name",
		.desc = "Servername for HostName TLS extension",
		.type = OPTION_ARG,
		.opt.arg = &cfg.tlsextcbp.servername,
	},
	{
		.name = "servername_fatal",
		.desc = "On mismatch send fatal alert (default warning alert)",
		.type = OPTION_VALUE,
		.opt.value = &cfg.tlsextcbp.extension_error,
		.value = SSL_TLSEXT_ERR_ALERT_FATAL,
	},
	{
		.name = "serverpref",
		.desc = "Use server's cipher preferences",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.off,
		.value = SSL_OP_CIPHER_SERVER_PREFERENCE,
	},
	{
		.name = "state",
		.desc = "Print the SSL states",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.state,
	},
	{
		.name = "status",
		.desc = "Respond to certificate status requests",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.tlsextstatus,
	},
	{
		.name = "status_timeout",
		.argname = "nsec",
		.desc = "Status request responder timeout",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = s_server_opt_status_timeout,
	},
	{
		.name = "status_url",
		.argname = "url",
		.desc = "Status request fallback URL",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = s_server_opt_status_url,
	},
	{
		.name = "status_verbose",
		.desc = "Enable status request verbose printout",
		.type = OPTION_FUNC,
		.opt.func = s_server_opt_status_verbose,
	},
#ifndef OPENSSL_NO_DTLS
	{
		.name = "timeout",
		.desc = "Enable timeouts",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.enable_timeouts,
	},
#endif
	{
		.name = "tls1_2",
		.desc = "Just talk TLSv1.2",
		.type = OPTION_FUNC,
		.opt.func = s_server_opt_protocol_version_tls1_2,
	},
	{
		.name = "tls1_3",
		.desc = "Just talk TLSv1.3",
		.type = OPTION_FUNC,
		.opt.func = s_server_opt_protocol_version_tls1_3,
	},
	{
		.name = "tlsextdebug",
		.desc = "Hex dump of all TLS extensions received",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.tlsextdebug,
	},
#ifndef OPENSSL_NO_SRTP
	{
		.name = "use_srtp",
		.argname = "profiles",
		.desc = "Offer SRTP key management with a colon-separated profile list",
		.type = OPTION_ARG,
		.opt.arg = &cfg.srtp_profiles,
	},
#endif
	{
		.name = "Verify",
		.argname = "depth",
		.desc = "Turn on peer certificate verification, must have a cert",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = s_server_opt_verify_fail,
	},
	{
		.name = "verify",
		.argname = "depth",
		.desc = "Turn on peer certificate verification",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = s_server_opt_verify,
	},
	{
		.name = "verify_return_error",
		.desc = "Return verification error",
		.type = OPTION_FLAG,
		.opt.flag = &verify_return_error,
	},
	{
		.name = "WWW",
		.desc = "Respond to a 'GET /<path> HTTP/1.0' with file ./<path>",
		.type = OPTION_VALUE,
		.opt.value = &cfg.www,
		.value = 2,
	},
	{
		.name = "www",
		.desc = "Respond to a 'GET /' with a status page",
		.type = OPTION_VALUE,
		.opt.value = &cfg.www,
		.value = 1,
	},
	{
		.name = NULL,
		.desc = "",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = s_server_opt_verify_param,
	},
	{ NULL },
};

static void
s_server_init(void)
{
	accept_socket = -1;
	cfg.cipher = NULL;
	cfg.server_verify = SSL_VERIFY_NONE;
	cfg.dcert_file = NULL;
	cfg.dkey_file = NULL;
	cfg.cert_file = TEST_CERT;
	cfg.key_file = NULL;
	cfg.cert_file2 = TEST_CERT2;
	cfg.key_file2 = NULL;
	ctx2 = NULL;
	cfg.nbio = 0;
	cfg.nbio_test = 0;
	ctx = NULL;
	cfg.www = 0;

	bio_s_out = NULL;
	cfg.debug = 0;
	cfg.msg = 0;
	cfg.quiet = 0;
}

static void
sv_usage(void)
{
	fprintf(stderr, "usage: s_server "
	    "[-accept port] [-alpn protocols] [-bugs] [-CAfile file]\n"
	    "    [-CApath directory] [-cert file] [-cert2 file]\n"
	    "    [-certform der | pem] [-cipher cipherlist]\n"
	    "    [-context id] [-crl_check] [-crl_check_all] [-crlf]\n"
	    "    [-dcert file] [-dcertform der | pem] [-debug]\n"
	    "    [-dhparam file] [-dkey file] [-dkeyform der | pem]\n"
	    "    [-dpass arg] [-dtls] [-dtls1_2] [-groups list] [-HTTP]\n"
	    "    [-id_prefix arg] [-key keyfile] [-key2 keyfile]\n"
	    "    [-keyform der | pem] [-keymatexport label]\n"
	    "    [-keymatexportlen len] [-msg] [-mtu mtu] [-naccept num]\n"
	    "    [-named_curve arg] [-nbio] [-nbio_test] [-no_cache]\n"
	    "    [-no_dhe] [-no_ecdhe] [-no_ticket] \n"
	    "    [-no_tls1_2] [-no_tls1_3] [-no_tmp_rsa]\n"
	    "    [-nocert] [-pass arg] [-quiet] [-servername name]\n"
	    "    [-servername_fatal] [-serverpref] [-state] [-status]\n"
	    "    [-status_timeout nsec] [-status_url url]\n"
	    "    [-status_verbose] [-timeout] \n"
	    "    [-tls1_2] [-tls1_3] [-tlsextdebug] [-use_srtp profiles]\n"
	    "    [-Verify depth] [-verify depth] [-verify_return_error]\n"
	    "    [-WWW] [-www]\n");
	fprintf(stderr, "\n");
	options_usage(s_server_options);
	fprintf(stderr, "\n");
}

int
s_server_main(int argc, char *argv[])
{
	int ret = 1;
	char *pass = NULL;
	char *dpass = NULL;
	X509 *s_cert = NULL, *s_dcert = NULL;
	EVP_PKEY *s_key = NULL, *s_dkey = NULL;
	EVP_PKEY *s_key2 = NULL;
	X509 *s_cert2 = NULL;
	tlsextalpnctx alpn_ctx = { NULL, 0 };

	if (pledge("stdio rpath inet dns tty", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.keymatexportlen = 20;
	cfg.meth = TLS_server_method();
	cfg.naccept = -1;
	cfg.port = PORT;
	cfg.cert_file = TEST_CERT;
	cfg.cert_file2 = TEST_CERT2;
	cfg.cert_format = FORMAT_PEM;
	cfg.dcert_format = FORMAT_PEM;
	cfg.dkey_format = FORMAT_PEM;
	cfg.key_format = FORMAT_PEM;
	cfg.server_verify = SSL_VERIFY_NONE;
	cfg.socket_type = SOCK_STREAM;
	cfg.tlscstatp.timeout = -1;
	cfg.tlsextcbp.extension_error =
	    SSL_TLSEXT_ERR_ALERT_WARNING;

	local_argc = argc;
	local_argv = argv;

	s_server_init();

	verify_depth = 0;

	if (options_parse(argc, argv, s_server_options, NULL, NULL) != 0) {
		if (cfg.errstr == NULL)
			sv_usage();
		goto end;
	}

	if (!app_passwd(bio_err, cfg.passarg,
	    cfg.dpassarg, &pass, &dpass)) {
		BIO_printf(bio_err, "Error getting password\n");
		goto end;
	}
	if (cfg.key_file == NULL)
		cfg.key_file = cfg.cert_file;
	if (cfg.key_file2 == NULL)
		cfg.key_file2 = cfg.cert_file2;

	if (cfg.nocert == 0) {
		s_key = load_key(bio_err, cfg.key_file,
		    cfg.key_format, 0, pass,
		    "server certificate private key file");
		if (!s_key) {
			ERR_print_errors(bio_err);
			goto end;
		}
		s_cert = load_cert(bio_err, cfg.cert_file,
		    cfg.cert_format,
		    NULL, "server certificate file");

		if (!s_cert) {
			ERR_print_errors(bio_err);
			goto end;
		}
		if (cfg.tlsextcbp.servername) {
			s_key2 = load_key(bio_err, cfg.key_file2,
			    cfg.key_format, 0, pass,
			    "second server certificate private key file");
			if (!s_key2) {
				ERR_print_errors(bio_err);
				goto end;
			}
			s_cert2 = load_cert(bio_err, cfg.cert_file2,
			    cfg.cert_format,
			    NULL, "second server certificate file");

			if (!s_cert2) {
				ERR_print_errors(bio_err);
				goto end;
			}
		}
	}
	alpn_ctx.data = NULL;
	if (cfg.alpn_in) {
		unsigned short len;
		alpn_ctx.data = next_protos_parse(&len,
		    cfg.alpn_in);
		if (alpn_ctx.data == NULL)
			goto end;
		alpn_ctx.len = len;
	}

	if (cfg.dcert_file) {

		if (cfg.dkey_file == NULL)
			cfg.dkey_file = cfg.dcert_file;

		s_dkey = load_key(bio_err, cfg.dkey_file,
		    cfg.dkey_format,
		    0, dpass, "second certificate private key file");
		if (!s_dkey) {
			ERR_print_errors(bio_err);
			goto end;
		}
		s_dcert = load_cert(bio_err, cfg.dcert_file,
		    cfg.dcert_format,
		    NULL, "second server certificate file");

		if (!s_dcert) {
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	if (bio_s_out == NULL) {
		if (cfg.quiet && !cfg.debug &&
		    !cfg.msg) {
			bio_s_out = BIO_new(BIO_s_null());
		} else {
			if (bio_s_out == NULL)
				bio_s_out = BIO_new_fp(stdout, BIO_NOCLOSE);
		}
	}
	if (cfg.nocert) {
		cfg.cert_file = NULL;
		cfg.key_file = NULL;
		cfg.dcert_file = NULL;
		cfg.dkey_file = NULL;
		cfg.cert_file2 = NULL;
		cfg.key_file2 = NULL;
	}
	ctx = SSL_CTX_new(cfg.meth);
	if (ctx == NULL) {
		ERR_print_errors(bio_err);
		goto end;
	}

	SSL_CTX_clear_mode(ctx, SSL_MODE_AUTO_RETRY);

	if (!SSL_CTX_set_min_proto_version(ctx, cfg.min_version))
		goto end;
	if (!SSL_CTX_set_max_proto_version(ctx, cfg.max_version))
		goto end;

	if (cfg.session_id_prefix) {
		if (strlen(cfg.session_id_prefix) >= 32)
			BIO_printf(bio_err,
			    "warning: id_prefix is too long, only one new session will be possible\n");
		else if (strlen(cfg.session_id_prefix) >= 16)
			BIO_printf(bio_err,
			    "warning: id_prefix is too long if you use SSLv2\n");
		if (!SSL_CTX_set_generate_session_id(ctx, generate_session_id)) {
			BIO_printf(bio_err, "error setting 'id_prefix'\n");
			ERR_print_errors(bio_err);
			goto end;
		}
		BIO_printf(bio_err, "id_prefix '%s' set.\n",
		    cfg.session_id_prefix);
	}
	SSL_CTX_set_quiet_shutdown(ctx, 1);
	if (cfg.bugs)
		SSL_CTX_set_options(ctx, SSL_OP_ALL);
	SSL_CTX_set_options(ctx, cfg.off);

	if (cfg.state)
		SSL_CTX_set_info_callback(ctx, apps_ssl_info_callback);
	if (cfg.no_cache)
		SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
	else
		SSL_CTX_sess_set_cache_size(ctx, 128);

#ifndef OPENSSL_NO_SRTP
	if (cfg.srtp_profiles != NULL)
		SSL_CTX_set_tlsext_use_srtp(ctx, cfg.srtp_profiles);
#endif

	if ((!SSL_CTX_load_verify_locations(ctx, cfg.CAfile,
	    cfg.CApath)) ||
	    (!SSL_CTX_set_default_verify_paths(ctx))) {
		/* BIO_printf(bio_err,"X509_load_verify_locations\n"); */
		ERR_print_errors(bio_err);
		/* goto end; */
	}
	if (cfg.vpm)
		SSL_CTX_set1_param(ctx, cfg.vpm);

	if (s_cert2) {
		ctx2 = SSL_CTX_new(cfg.meth);
		if (ctx2 == NULL) {
			ERR_print_errors(bio_err);
			goto end;
		}

		if (!SSL_CTX_set_min_proto_version(ctx2,
		    cfg.min_version))
			goto end;
		if (!SSL_CTX_set_max_proto_version(ctx2,
		    cfg.max_version))
			goto end;
		SSL_CTX_clear_mode(ctx2, SSL_MODE_AUTO_RETRY);
	}
	if (ctx2) {
		BIO_printf(bio_s_out, "Setting secondary ctx parameters\n");

		if (cfg.session_id_prefix) {
			if (strlen(cfg.session_id_prefix) >= 32)
				BIO_printf(bio_err,
				    "warning: id_prefix is too long, only one new session will be possible\n");
			else if (strlen(cfg.session_id_prefix) >= 16)
				BIO_printf(bio_err,
				    "warning: id_prefix is too long if you use SSLv2\n");
			if (!SSL_CTX_set_generate_session_id(ctx2,
			    generate_session_id)) {
				BIO_printf(bio_err,
				    "error setting 'id_prefix'\n");
				ERR_print_errors(bio_err);
				goto end;
			}
			BIO_printf(bio_err, "id_prefix '%s' set.\n",
			    cfg.session_id_prefix);
		}
		SSL_CTX_set_quiet_shutdown(ctx2, 1);
		if (cfg.bugs)
			SSL_CTX_set_options(ctx2, SSL_OP_ALL);
		SSL_CTX_set_options(ctx2, cfg.off);

		if (cfg.state)
			SSL_CTX_set_info_callback(ctx2, apps_ssl_info_callback);

		if (cfg.no_cache)
			SSL_CTX_set_session_cache_mode(ctx2, SSL_SESS_CACHE_OFF);
		else
			SSL_CTX_sess_set_cache_size(ctx2, 128);

		if ((!SSL_CTX_load_verify_locations(ctx2,
		    cfg.CAfile, cfg.CApath)) ||
		    (!SSL_CTX_set_default_verify_paths(ctx2))) {
			ERR_print_errors(bio_err);
		}
		if (cfg.vpm)
			SSL_CTX_set1_param(ctx2, cfg.vpm);
	}
	if (alpn_ctx.data)
		SSL_CTX_set_alpn_select_cb(ctx, alpn_cb, &alpn_ctx);

	if (cfg.groups_in != NULL) {
		if (SSL_CTX_set1_groups_list(ctx, cfg.groups_in) != 1) {
			BIO_printf(bio_err, "Failed to set groups '%s'\n",
			    cfg.groups_in);
			goto end;
		}
	}

#ifndef OPENSSL_NO_DH
	if (!cfg.no_dhe) {
		DH *dh = NULL;

		if (cfg.dhfile)
			dh = load_dh_param(cfg.dhfile);
		else if (cfg.cert_file)
			dh = load_dh_param(cfg.cert_file);

		if (dh != NULL)
			BIO_printf(bio_s_out, "Setting temp DH parameters\n");
		else
			BIO_printf(bio_s_out, "Using auto DH parameters\n");
		(void) BIO_flush(bio_s_out);

		if (dh == NULL)
			SSL_CTX_set_dh_auto(ctx, 1);
		else if (!SSL_CTX_set_tmp_dh(ctx, dh)) {
			BIO_printf(bio_err,
			    "Error setting temp DH parameters\n");
			ERR_print_errors(bio_err);
			DH_free(dh);
			goto end;
		}

		if (ctx2) {
			if (!cfg.dhfile) {
				DH *dh2 = NULL;

				if (cfg.cert_file2 != NULL)
					dh2 = load_dh_param(
					    cfg.cert_file2);
				if (dh2 != NULL) {
					BIO_printf(bio_s_out,
					    "Setting temp DH parameters\n");
					(void) BIO_flush(bio_s_out);

					DH_free(dh);
					dh = dh2;
				}
			}
			if (dh == NULL)
				SSL_CTX_set_dh_auto(ctx2, 1);
			else if (!SSL_CTX_set_tmp_dh(ctx2, dh)) {
				BIO_printf(bio_err,
				    "Error setting temp DH parameters\n");
				ERR_print_errors(bio_err);
				DH_free(dh);
				goto end;
			}
		}
		DH_free(dh);
	}
#endif

	if (!cfg.no_ecdhe && cfg.named_curve != NULL) {
		EC_KEY *ecdh = NULL;
		int nid;

		if ((nid = OBJ_sn2nid(cfg.named_curve)) == 0) {
			BIO_printf(bio_err, "unknown curve name (%s)\n",
			    cfg.named_curve);
			goto end;
 		}
		if ((ecdh = EC_KEY_new_by_curve_name(nid)) == NULL) {
			BIO_printf(bio_err, "unable to create curve (%s)\n",
			    cfg.named_curve);
			goto end;
 		}
		BIO_printf(bio_s_out, "Setting temp ECDH parameters\n");
		(void) BIO_flush(bio_s_out);

		SSL_CTX_set_tmp_ecdh(ctx, ecdh);
		if (ctx2)
			SSL_CTX_set_tmp_ecdh(ctx2, ecdh);
		EC_KEY_free(ecdh);
	}

	if (!set_cert_key_stuff(ctx, s_cert, s_key))
		goto end;
	if (ctx2 && !set_cert_key_stuff(ctx2, s_cert2, s_key2))
		goto end;
	if (s_dcert != NULL) {
		if (!set_cert_key_stuff(ctx, s_dcert, s_dkey))
			goto end;
	}

	if (cfg.cipher != NULL) {
		if (!SSL_CTX_set_cipher_list(ctx, cfg.cipher)) {
			BIO_printf(bio_err, "error setting cipher list\n");
			ERR_print_errors(bio_err);
			goto end;
		}
		if (ctx2 && !SSL_CTX_set_cipher_list(ctx2,
		    cfg.cipher)) {
			BIO_printf(bio_err, "error setting cipher list\n");
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	SSL_CTX_set_verify(ctx, cfg.server_verify, verify_callback);
	SSL_CTX_set_session_id_context(ctx,
	    (void *) &s_server_session_id_context,
	    sizeof s_server_session_id_context);

	/* Set DTLS cookie generation and verification callbacks */
	SSL_CTX_set_cookie_generate_cb(ctx, generate_cookie_callback);
	SSL_CTX_set_cookie_verify_cb(ctx, verify_cookie_callback);

	if (ctx2) {
		SSL_CTX_set_verify(ctx2, cfg.server_verify,
		    verify_callback);
		SSL_CTX_set_session_id_context(ctx2,
		    (void *) &s_server_session_id_context,
		    sizeof s_server_session_id_context);

		cfg.tlsextcbp.biodebug = bio_s_out;
		SSL_CTX_set_tlsext_servername_callback(ctx2, ssl_servername_cb);
		SSL_CTX_set_tlsext_servername_arg(ctx2,
		    &cfg.tlsextcbp);
		SSL_CTX_set_tlsext_servername_callback(ctx, ssl_servername_cb);
		SSL_CTX_set_tlsext_servername_arg(ctx,
		    &cfg.tlsextcbp);
	}

	if (cfg.CAfile != NULL) {
		SSL_CTX_set_client_CA_list(ctx,
		    SSL_load_client_CA_file(cfg.CAfile));
		if (ctx2)
			SSL_CTX_set_client_CA_list(ctx2,
			    SSL_load_client_CA_file(cfg.CAfile));
	}
	BIO_printf(bio_s_out, "ACCEPT\n");
	(void) BIO_flush(bio_s_out);
	if (cfg.www)
		do_server(cfg.port, cfg.socket_type,
		    &accept_socket, www_body, cfg.context,
		    cfg.naccept);
	else
		do_server(cfg.port, cfg.socket_type,
		    &accept_socket, sv_body, cfg.context,
		    cfg.naccept);
	print_stats(bio_s_out, ctx);
	ret = 0;
 end:
	SSL_CTX_free(ctx);
	X509_free(s_cert);
	X509_free(s_dcert);
	EVP_PKEY_free(s_key);
	EVP_PKEY_free(s_dkey);
	free(pass);
	free(dpass);
	X509_VERIFY_PARAM_free(cfg.vpm);
	free(cfg.tlscstatp.host);
	free(cfg.tlscstatp.port);
	free(cfg.tlscstatp.path);
	SSL_CTX_free(ctx2);
	X509_free(s_cert2);
	EVP_PKEY_free(s_key2);
	free(alpn_ctx.data);
	if (bio_s_out != NULL) {
		BIO_free(bio_s_out);
		bio_s_out = NULL;
	}

	return (ret);
}

static void
print_stats(BIO *bio, SSL_CTX *ssl_ctx)
{
	BIO_printf(bio, "%4ld items in the session cache\n",
	    SSL_CTX_sess_number(ssl_ctx));
	BIO_printf(bio, "%4ld client connects (SSL_connect())\n",
	    SSL_CTX_sess_connect(ssl_ctx));
	BIO_printf(bio, "%4ld client renegotiates (SSL_connect())\n",
	    SSL_CTX_sess_connect_renegotiate(ssl_ctx));
	BIO_printf(bio, "%4ld client connects that finished\n",
	    SSL_CTX_sess_connect_good(ssl_ctx));
	BIO_printf(bio, "%4ld server accepts (SSL_accept())\n",
	    SSL_CTX_sess_accept(ssl_ctx));
	BIO_printf(bio, "%4ld server renegotiates (SSL_accept())\n",
	    SSL_CTX_sess_accept_renegotiate(ssl_ctx));
	BIO_printf(bio, "%4ld server accepts that finished\n",
	    SSL_CTX_sess_accept_good(ssl_ctx));
	BIO_printf(bio, "%4ld session cache hits\n",
	    SSL_CTX_sess_hits(ssl_ctx));
	BIO_printf(bio, "%4ld session cache misses\n",
	    SSL_CTX_sess_misses(ssl_ctx));
	BIO_printf(bio, "%4ld session cache timeouts\n",
	    SSL_CTX_sess_timeouts(ssl_ctx));
	BIO_printf(bio, "%4ld callback cache hits\n",
	    SSL_CTX_sess_cb_hits(ssl_ctx));
	BIO_printf(bio, "%4ld cache full overflows (%ld allowed)\n",
	    SSL_CTX_sess_cache_full(ssl_ctx),
	    SSL_CTX_sess_get_cache_size(ssl_ctx));
}

static int
sv_body(int s, unsigned char *context)
{
	char *buf = NULL;
	int ret = 1;
	int k, i;
	unsigned long l;
	SSL *con = NULL;
	BIO *sbio;
	struct timeval timeout;

	if ((buf = malloc(bufsize)) == NULL) {
		BIO_printf(bio_err, "out of memory\n");
		goto err;
	}
	if (cfg.nbio) {
		if (!cfg.quiet)
			BIO_printf(bio_err, "turning on non blocking io\n");
		if (!BIO_socket_nbio(s, 1))
			ERR_print_errors(bio_err);
	}

	if (con == NULL) {
		con = SSL_new(ctx);
		if (cfg.tlsextdebug) {
			SSL_set_tlsext_debug_callback(con, tlsext_cb);
			SSL_set_tlsext_debug_arg(con, bio_s_out);
		}
		if (cfg.tlsextstatus) {
			SSL_CTX_set_tlsext_status_cb(ctx, cert_status_cb);
			cfg.tlscstatp.err = bio_err;
			SSL_CTX_set_tlsext_status_arg(ctx,
			    &cfg.tlscstatp);
		}
		if (context)
			SSL_set_session_id_context(con, context,
			    strlen((char *) context));
	}
	SSL_clear(con);

	if (SSL_is_dtls(con)) {
		sbio = BIO_new_dgram(s, BIO_NOCLOSE);

		if (cfg.enable_timeouts) {
			timeout.tv_sec = 0;
			timeout.tv_usec = DGRAM_RCV_TIMEOUT;
			BIO_ctrl(sbio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0,
			    &timeout);

			timeout.tv_sec = 0;
			timeout.tv_usec = DGRAM_SND_TIMEOUT;
			BIO_ctrl(sbio, BIO_CTRL_DGRAM_SET_SEND_TIMEOUT, 0,
			    &timeout);
		}
		if (cfg.socket_mtu > 28) {
			SSL_set_options(con, SSL_OP_NO_QUERY_MTU);
			SSL_set_mtu(con, cfg.socket_mtu - 28);
		} else
			/* want to do MTU discovery */
			BIO_ctrl(sbio, BIO_CTRL_DGRAM_MTU_DISCOVER, 0, NULL);

		/* turn on cookie exchange */
		SSL_set_options(con, SSL_OP_COOKIE_EXCHANGE);
	} else
		sbio = BIO_new_socket(s, BIO_NOCLOSE);

	if (cfg.nbio_test) {
		BIO *test;

		test = BIO_new(BIO_f_nbio_test());
		sbio = BIO_push(test, sbio);
	}

	SSL_set_bio(con, sbio, sbio);
	SSL_set_accept_state(con);
	/* SSL_set_fd(con,s); */

	if (cfg.debug) {
		SSL_set_debug(con, 1);
		BIO_set_callback(SSL_get_rbio(con), bio_dump_callback);
		BIO_set_callback_arg(SSL_get_rbio(con), (char *) bio_s_out);
	}
	if (cfg.msg) {
		SSL_set_msg_callback(con, msg_cb);
		SSL_set_msg_callback_arg(con, bio_s_out);
	}
	if (cfg.tlsextdebug) {
		SSL_set_tlsext_debug_callback(con, tlsext_cb);
		SSL_set_tlsext_debug_arg(con, bio_s_out);
	}

	for (;;) {
		int read_from_terminal;
		int read_from_sslcon;
		struct pollfd pfd[2];
		int ptimeout;

		read_from_terminal = 0;
		read_from_sslcon = SSL_pending(con);

		if (!read_from_sslcon) {
			pfd[0].fd = fileno(stdin);
			pfd[0].events = POLLIN;
			pfd[1].fd = s;
			pfd[1].events = POLLIN;

			if (SSL_is_dtls(con) &&
			    DTLSv1_get_timeout(con, &timeout))
				ptimeout = timeout.tv_sec * 1000 +
				    timeout.tv_usec / 1000;
			else
				ptimeout = -1;

			i = poll(pfd, 2, ptimeout);

			if (SSL_is_dtls(con) &&
			    DTLSv1_handle_timeout(con) > 0)
				BIO_printf(bio_err, "TIMEOUT occured\n");
			if (i <= 0)
				continue;
			if (pfd[0].revents) {
				if ((pfd[0].revents & (POLLERR|POLLNVAL)))
					continue;
				read_from_terminal = 1;
			}
			if (pfd[1].revents) {
				if ((pfd[1].revents & (POLLERR|POLLNVAL)))
					continue;
				read_from_sslcon = 1;
			}
		}
		if (read_from_terminal) {
			if (cfg.crlf) {
				int j, lf_num;

				i = read(fileno(stdin), buf, bufsize / 2);
				lf_num = 0;
				/* both loops are skipped when i <= 0 */
				for (j = 0; j < i; j++)
					if (buf[j] == '\n')
						lf_num++;
				for (j = i - 1; j >= 0; j--) {
					buf[j + lf_num] = buf[j];
					if (buf[j] == '\n') {
						lf_num--;
						i++;
						buf[j + lf_num] = '\r';
					}
				}
				assert(lf_num == 0);
			} else
				i = read(fileno(stdin), buf, bufsize);
			if (!cfg.quiet) {
				if ((i <= 0) || (buf[0] == 'Q')) {
					BIO_printf(bio_s_out, "DONE\n");
					shutdown(s, SHUT_RD);
					close(s);
					close_accept_socket();
					ret = -11;
					goto err;
				}
				if ((i <= 0) || (buf[0] == 'q')) {
					BIO_printf(bio_s_out, "DONE\n");
					if (!SSL_is_dtls(con)) {
						shutdown(s, SHUT_RD);
						close(s);
					}
					/*
					 * close_accept_socket(); ret= -11;
					 */
					goto err;
				}
				if ((buf[0] == 'r') &&
				    ((buf[1] == '\n') || (buf[1] == '\r'))) {
					SSL_renegotiate(con);
					i = SSL_do_handshake(con);
					printf("SSL_do_handshake -> %d\n", i);
					i = 0;	/* 13; */
					continue;
					/*
					 * RE-NEGOTIATE\n");
					 */
				}
				if ((buf[0] == 'R') &&
				    ((buf[1] == '\n') || (buf[1] == '\r'))) {
					SSL_set_verify(con,
					    SSL_VERIFY_PEER |
					    SSL_VERIFY_CLIENT_ONCE,
					    NULL);
					SSL_renegotiate(con);
					i = SSL_do_handshake(con);
					printf("SSL_do_handshake -> %d\n", i);
					i = 0;	/* 13; */
					continue;
					/*
					 * RE-NEGOTIATE asking for client
					 * cert\n");
					 */
				}
				if (buf[0] == 'P') {
					static const char *str =
					    "Lets print some clear text\n";
					BIO_write(SSL_get_wbio(con), str,
					    strlen(str));
				}
				if (buf[0] == 'S') {
					print_stats(bio_s_out,
					    SSL_get_SSL_CTX(con));
				}
			}
			l = k = 0;
			for (;;) {
				/* should do a select for the write */
#ifdef RENEG
				{
					static count = 0;
					if (++count == 100) {
						count = 0;
						SSL_renegotiate(con);
					}
				}
#endif
				k = SSL_write(con, &(buf[l]), (unsigned int) i);
				switch (SSL_get_error(con, k)) {
				case SSL_ERROR_NONE:
					break;
				case SSL_ERROR_WANT_WRITE:
				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_WANT_X509_LOOKUP:
					BIO_printf(bio_s_out, "Write BLOCK\n");
					break;
				case SSL_ERROR_SYSCALL:
				case SSL_ERROR_SSL:
					BIO_printf(bio_s_out, "ERROR\n");
					ERR_print_errors(bio_err);
					ret = 1;
					goto err;
					/* break; */
				case SSL_ERROR_ZERO_RETURN:
					BIO_printf(bio_s_out, "DONE\n");
					ret = 1;
					goto err;
				}
				if (k <= 0)
					continue;
				l += k;
				i -= k;
				if (i <= 0)
					break;
			}
		}
		if (read_from_sslcon) {
			if (!SSL_is_init_finished(con)) {
				i = init_ssl_connection(con);

				if (i < 0) {
					ret = 0;
					goto err;
				} else if (i == 0) {
					ret = 1;
					goto err;
				}
			} else {
		again:
				i = SSL_read(con, (char *) buf, bufsize);
				switch (SSL_get_error(con, i)) {
				case SSL_ERROR_NONE: {
						int len, n;
						for (len = 0; len < i;) {
							do {
								n = write(fileno(stdout), buf + len, i - len);
							} while (n == -1 && errno == EINTR);

							if (n == -1) {
								BIO_printf(bio_s_out, "ERROR\n");
								goto err;
							}
							len += n;
						}
					}
					if (SSL_pending(con))
						goto again;
					break;
				case SSL_ERROR_WANT_WRITE:
				case SSL_ERROR_WANT_READ:
					BIO_printf(bio_s_out, "Read BLOCK\n");
					break;
				case SSL_ERROR_SYSCALL:
				case SSL_ERROR_SSL:
					BIO_printf(bio_s_out, "ERROR\n");
					ERR_print_errors(bio_err);
					ret = 1;
					goto err;
				case SSL_ERROR_ZERO_RETURN:
					BIO_printf(bio_s_out, "DONE\n");
					ret = 1;
					goto err;
				}
			}
		}
	}
 err:
	if (con != NULL) {
		BIO_printf(bio_s_out, "shutting down SSL\n");
		SSL_set_shutdown(con,
		    SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
		SSL_free(con);
	}
	BIO_printf(bio_s_out, "CONNECTION CLOSED\n");
	freezero(buf, bufsize);
	if (ret >= 0)
		BIO_printf(bio_s_out, "ACCEPT\n");
	return (ret);
}

static void
close_accept_socket(void)
{
	BIO_printf(bio_err, "shutdown accept socket\n");
	if (accept_socket >= 0) {
		shutdown(accept_socket, SHUT_RDWR);
		close(accept_socket);
	}
}

static int
init_ssl_connection(SSL *con)
{
	int i;
	const char *str;
	X509 *peer;
	long verify_error;
	char buf[BUFSIZ];
	unsigned char *exportedkeymat;

	i = SSL_accept(con);
	if (i <= 0) {
		if (BIO_sock_should_retry(i)) {
			BIO_printf(bio_s_out, "DELAY\n");
			return (1);
		}
		BIO_printf(bio_err, "ERROR\n");
		verify_error = SSL_get_verify_result(con);
		if (verify_error != X509_V_OK) {
			BIO_printf(bio_err, "verify error:%s\n",
			    X509_verify_cert_error_string(verify_error));
		} else
			ERR_print_errors(bio_err);
		return (0);
	}
	PEM_write_bio_SSL_SESSION(bio_s_out, SSL_get_session(con));

	peer = SSL_get_peer_certificate(con);
	if (peer != NULL) {
		BIO_printf(bio_s_out, "Client certificate\n");
		PEM_write_bio_X509(bio_s_out, peer);
		X509_NAME_oneline(X509_get_subject_name(peer), buf, sizeof buf);
		BIO_printf(bio_s_out, "subject=%s\n", buf);
		X509_NAME_oneline(X509_get_issuer_name(peer), buf, sizeof buf);
		BIO_printf(bio_s_out, "issuer=%s\n", buf);
		X509_free(peer);
	}
	if (SSL_get_shared_ciphers(con, buf, sizeof buf) != NULL)
		BIO_printf(bio_s_out, "Shared ciphers:%s\n", buf);
	str = SSL_CIPHER_get_name(SSL_get_current_cipher(con));
	BIO_printf(bio_s_out, "CIPHER is %s\n", (str != NULL) ? str : "(NONE)");

#ifndef OPENSSL_NO_SRTP
	{
		SRTP_PROTECTION_PROFILE *srtp_profile
		= SSL_get_selected_srtp_profile(con);

		if (srtp_profile)
			BIO_printf(bio_s_out,
			    "SRTP Extension negotiated, profile=%s\n",
			    srtp_profile->name);
	}
#endif
	if (SSL_cache_hit(con))
		BIO_printf(bio_s_out, "Reused session-id\n");
	BIO_printf(bio_s_out, "Secure Renegotiation IS%s supported\n",
	    SSL_get_secure_renegotiation_support(con) ? "" : " NOT");
	if (cfg.keymatexportlabel != NULL) {
		BIO_printf(bio_s_out, "Keying material exporter:\n");
		BIO_printf(bio_s_out, "    Label: '%s'\n",
		    cfg.keymatexportlabel);
		BIO_printf(bio_s_out, "    Length: %i bytes\n",
		    cfg.keymatexportlen);
		exportedkeymat = malloc(cfg.keymatexportlen);
		if (exportedkeymat != NULL) {
			if (!SSL_export_keying_material(con, exportedkeymat,
				cfg.keymatexportlen,
				cfg.keymatexportlabel,
				strlen(cfg.keymatexportlabel),
				NULL, 0, 0)) {
				BIO_printf(bio_s_out, "    Error\n");
			} else {
				BIO_printf(bio_s_out, "    Keying material: ");
				for (i = 0; i < cfg.keymatexportlen; i++)
					BIO_printf(bio_s_out, "%02X",
					    exportedkeymat[i]);
				BIO_printf(bio_s_out, "\n");
			}
			free(exportedkeymat);
		}
	}
	return (1);
}

#ifndef OPENSSL_NO_DH
static DH *
load_dh_param(const char *dhfile)
{
	DH *ret = NULL;
	BIO *bio;

	if ((bio = BIO_new_file(dhfile, "r")) == NULL)
		goto err;
	ret = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
 err:
	BIO_free(bio);
	return (ret);
}
#endif

static int
www_body(int s, unsigned char *context)
{
	char *buf = NULL;
	int ret = 1;
	int i, j, k, dot;
	SSL *con;
	const SSL_CIPHER *c;
	BIO *io, *ssl_bio, *sbio;

	buf = malloc(bufsize);
	if (buf == NULL)
		return (0);
	io = BIO_new(BIO_f_buffer());
	ssl_bio = BIO_new(BIO_f_ssl());
	if ((io == NULL) || (ssl_bio == NULL))
		goto err;

	if (cfg.nbio) {
		if (!cfg.quiet)
			BIO_printf(bio_err, "turning on non blocking io\n");
		if (!BIO_socket_nbio(s, 1))
			ERR_print_errors(bio_err);
	}

	/* lets make the output buffer a reasonable size */
	if (!BIO_set_write_buffer_size(io, bufsize))
		goto err;

	if ((con = SSL_new(ctx)) == NULL)
		goto err;
	if (cfg.tlsextdebug) {
		SSL_set_tlsext_debug_callback(con, tlsext_cb);
		SSL_set_tlsext_debug_arg(con, bio_s_out);
	}
	if (context)
		SSL_set_session_id_context(con, context,
		    strlen((char *) context));

	sbio = BIO_new_socket(s, BIO_NOCLOSE);
	if (cfg.nbio_test) {
		BIO *test;

		test = BIO_new(BIO_f_nbio_test());
		sbio = BIO_push(test, sbio);
	}
	SSL_set_bio(con, sbio, sbio);
	SSL_set_accept_state(con);

	/* SSL_set_fd(con,s); */
	BIO_set_ssl(ssl_bio, con, BIO_CLOSE);
	BIO_push(io, ssl_bio);

	if (cfg.debug) {
		SSL_set_debug(con, 1);
		BIO_set_callback(SSL_get_rbio(con), bio_dump_callback);
		BIO_set_callback_arg(SSL_get_rbio(con), (char *) bio_s_out);
	}
	if (cfg.msg) {
		SSL_set_msg_callback(con, msg_cb);
		SSL_set_msg_callback_arg(con, bio_s_out);
	}
	for (;;) {
		i = BIO_gets(io, buf, bufsize - 1);
		if (i < 0) {	/* error */
			if (!BIO_should_retry(io)) {
				if (!cfg.quiet)
					ERR_print_errors(bio_err);
				goto err;
			} else {
				if (cfg.debug)  {
					BIO_printf(bio_s_out, "read R BLOCK\n");
					sleep(1);
				}
				continue;
			}
		} else if (i == 0) {	/* end of input */
			ret = 1;
			goto end;
		}
		/* else we have data */
		if (((cfg.www == 1) &&
		    (strncmp("GET ", buf, 4) == 0)) ||
		    ((cfg.www == 2) &&
		    (strncmp("GET /stats ", buf, 11) == 0))) {
			char *p;
			X509 *peer;
			STACK_OF(SSL_CIPHER) *sk;
			static const char *space = "                          ";

			BIO_puts(io, "HTTP/1.0 200 ok\r\nContent-type: text/html\r\n\r\n");
			BIO_puts(io, "<HTML><BODY BGCOLOR=\"#ffffff\">\n");
			BIO_puts(io, "<pre>\n");
/*			BIO_puts(io,SSLeay_version(SSLEAY_VERSION));*/
			BIO_puts(io, "\n");
			for (i = 0; i < local_argc; i++) {
				BIO_puts(io, local_argv[i]);
				BIO_write(io, " ", 1);
			}
			BIO_puts(io, "\n");

			BIO_printf(io,
			    "Secure Renegotiation IS%s supported\n",
			    SSL_get_secure_renegotiation_support(con) ?
			    "" : " NOT");

			/*
			 * The following is evil and should not really be
			 * done
			 */
			BIO_printf(io,
			    "Ciphers supported in s_server binary\n");
			sk = SSL_get_ciphers(con);
			j = sk_SSL_CIPHER_num(sk);
			for (i = 0; i < j; i++) {
				c = sk_SSL_CIPHER_value(sk, i);
				BIO_printf(io, "%-11s:%-25s",
				    SSL_CIPHER_get_version(c),
				    SSL_CIPHER_get_name(c));
				if ((((i + 1) % 2) == 0) && (i + 1 != j))
					BIO_puts(io, "\n");
			}
			BIO_puts(io, "\n");
			p = SSL_get_shared_ciphers(con, buf, bufsize);
			if (p != NULL) {
				BIO_printf(io,
				    "---\nCiphers common between both SSL end points:\n");
				j = i = 0;
				while (*p) {
					if (*p == ':') {
						BIO_write(io, space, 26 - j);
						i++;
						j = 0;
						BIO_write(io,
						    ((i % 3) ?  " " : "\n"), 1);
					} else {
						BIO_write(io, p, 1);
						j++;
					}
					p++;
				}
				BIO_puts(io, "\n");
			}
			BIO_printf(io, (SSL_cache_hit(con)
				? "---\nReused, "
				: "---\nNew, "));
			c = SSL_get_current_cipher(con);
			BIO_printf(io, "%s, Cipher is %s\n",
			    SSL_CIPHER_get_version(c),
			    SSL_CIPHER_get_name(c));
			SSL_SESSION_print(io, SSL_get_session(con));
			BIO_printf(io, "---\n");
			print_stats(io, SSL_get_SSL_CTX(con));
			BIO_printf(io, "---\n");
			peer = SSL_get_peer_certificate(con);
			if (peer != NULL) {
				BIO_printf(io, "Client certificate\n");
				X509_print(io, peer);
				PEM_write_bio_X509(io, peer);
			} else
				BIO_puts(io,
				    "no client certificate available\n");
			BIO_puts(io, "</BODY></HTML>\r\n\r\n");
			break;
		} else if ((cfg.www == 2 ||
		    cfg.www == 3) &&
		    (strncmp("GET /", buf, 5) == 0)) {
			BIO *file;
			char *p, *e;
			static const char *text = "HTTP/1.0 200 ok\r\nContent-type: text/plain\r\n\r\n";

			/* skip the '/' */
			p = &(buf[5]);

			dot = 1;
			for (e = p; *e != '\0'; e++) {
				if (e[0] == ' ')
					break;

				switch (dot) {
				case 1:
					dot = (e[0] == '.') ? 2 : 0;
					break;
				case 2:
					dot = (e[0] == '.') ? 3 : 0;
					break;
				case 3:
					dot = (e[0] == '/' || e[0] == '\\') ?
					    -1 : 0;
					break;
				}
				if (dot == 0)
					dot = (e[0] == '/' || e[0] == '\\') ?
					    1 : 0;
			}
			dot = (dot == 3) || (dot == -1);  /* filename contains
							   * ".." component */

			if (*e == '\0') {
				BIO_puts(io, text);
				BIO_printf(io,
				    "'%s' is an invalid file name\r\n", p);
				break;
			}
			*e = '\0';

			if (dot) {
				BIO_puts(io, text);
				BIO_printf(io,
				    "'%s' contains '..' reference\r\n", p);
				break;
			}
			if (*p == '/') {
				BIO_puts(io, text);
				BIO_printf(io,
				    "'%s' is an invalid path\r\n", p);
				break;
			}
			/* if a directory, do the index thang */
			if (app_isdir(p) > 0) {
				BIO_puts(io, text);
				BIO_printf(io, "'%s' is a directory\r\n", p);
				break;
			}
			if ((file = BIO_new_file(p, "r")) == NULL) {
				BIO_puts(io, text);
				BIO_printf(io, "Error opening '%s'\r\n", p);
				ERR_print_errors(io);
				break;
			}
			if (!cfg.quiet)
				BIO_printf(bio_err, "FILE:%s\n", p);

			if (cfg.www == 2) {
				i = strlen(p);
				if (((i > 5) && (strcmp(&(p[i - 5]), ".html") == 0)) ||
				    ((i > 4) && (strcmp(&(p[i - 4]), ".php") == 0)) ||
				    ((i > 4) && (strcmp(&(p[i - 4]), ".htm") == 0)))
					BIO_puts(io, "HTTP/1.0 200 ok\r\nContent-type: text/html\r\n\r\n");
				else
					BIO_puts(io, "HTTP/1.0 200 ok\r\nContent-type: text/plain\r\n\r\n");
			}
			/* send the file */
			for (;;) {
				i = BIO_read(file, buf, bufsize);
				if (i <= 0)
					break;

#ifdef RENEG
				total_bytes += i;
				fprintf(stderr, "%d\n", i);
				if (total_bytes > 3 * 1024) {
					total_bytes = 0;
					fprintf(stderr, "RENEGOTIATE\n");
					SSL_renegotiate(con);
				}
#endif

				for (j = 0; j < i;) {
#ifdef RENEG
					{
						static count = 0;
						if (++count == 13) {
							SSL_renegotiate(con);
						}
					}
#endif
					k = BIO_write(io, &(buf[j]), i - j);
					if (k <= 0) {
						if (!BIO_should_retry(io))
							goto write_error;
						else {
							BIO_printf(bio_s_out,
							    "rwrite W BLOCK\n");
						}
					} else {
						j += k;
					}
				}
			}
	write_error:
			BIO_free(file);
			break;
		}
	}

	for (;;) {
		i = (int) BIO_flush(io);
		if (i <= 0) {
			if (!BIO_should_retry(io))
				break;
		} else
			break;
	}
 end:
	/* make sure we re-use sessions */
	SSL_set_shutdown(con, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);

 err:

	if (ret >= 0)
		BIO_printf(bio_s_out, "ACCEPT\n");

	free(buf);
	BIO_free_all(io);
/*	if (ssl_bio != NULL) BIO_free(ssl_bio);*/
	return (ret);
}

#define MAX_SESSION_ID_ATTEMPTS 10
static int
generate_session_id(const SSL *ssl, unsigned char *id, unsigned int *id_len)
{
	unsigned int count = 0;
	do {
		arc4random_buf(id, *id_len);
		/*
		 * Prefix the session_id with the required prefix. NB: If our
		 * prefix is too long, clip it - but there will be worse
		 * effects anyway, eg. the server could only possibly create
		 * 1 session ID (ie. the prefix!) so all future session
		 * negotiations will fail due to conflicts.
		 */
		memcpy(id, cfg.session_id_prefix,
		    (strlen(cfg.session_id_prefix) < *id_len) ?
		    strlen(cfg.session_id_prefix) : *id_len);
	}
	while (SSL_has_matching_session_id(ssl, id, *id_len) &&
	    (++count < MAX_SESSION_ID_ATTEMPTS));
	if (count >= MAX_SESSION_ID_ATTEMPTS)
		return 0;
	return 1;
}

static int
ssl_servername_cb(SSL *s, int *ad, void *arg)
{
	tlsextctx *p = (tlsextctx *) arg;
	const char *servername = SSL_get_servername(s,
	    TLSEXT_NAMETYPE_host_name);

	if (servername && p->biodebug)
		BIO_printf(p->biodebug, "Hostname in TLS extension: \"%s\"\n",
		    servername);

	if (!p->servername)
		return SSL_TLSEXT_ERR_NOACK;

	if (servername) {
		if (strcmp(servername, p->servername))
			return p->extension_error;
		if (ctx2) {
			BIO_printf(p->biodebug, "Switching server context.\n");
			SSL_set_SSL_CTX(s, ctx2);
		}
	}
	return SSL_TLSEXT_ERR_OK;
}

/* Certificate Status callback. This is called when a client includes a
 * certificate status request extension.
 *
 * This is a simplified version. It examines certificates each time and
 * makes one OCSP responder query for each request.
 *
 * A full version would store details such as the OCSP certificate IDs and
 * minimise the number of OCSP responses by caching them until they were
 * considered "expired".
 */

static int
cert_status_cb(SSL *s, void *arg)
{
	tlsextstatusctx *srctx = arg;
	BIO *err = srctx->err;
	char *host = NULL, *port = NULL, *path = NULL;
	int use_ssl;
	unsigned char *rspder = NULL;
	int rspderlen;
	STACK_OF(OPENSSL_STRING) *aia = NULL;
	X509 *x = NULL;
	X509_STORE_CTX *inctx = NULL;
	X509_OBJECT *obj = NULL;
	OCSP_REQUEST *req = NULL;
	OCSP_RESPONSE *resp = NULL;
	OCSP_CERTID *id = NULL;
	STACK_OF(X509_EXTENSION) *exts;
	int ret = SSL_TLSEXT_ERR_NOACK;
	int i;

	if (srctx->verbose)
		BIO_puts(err, "cert_status: callback called\n");
	/* Build up OCSP query from server certificate */
	x = SSL_get_certificate(s);
	aia = X509_get1_ocsp(x);
	if (aia) {
		if (!OCSP_parse_url(sk_OPENSSL_STRING_value(aia, 0),
		    &host, &port, &path, &use_ssl)) {
			BIO_puts(err, "cert_status: can't parse AIA URL\n");
			goto err;
		}
		if (srctx->verbose)
			BIO_printf(err, "cert_status: AIA URL: %s\n",
			    sk_OPENSSL_STRING_value(aia, 0));
	} else {
		if (!srctx->host) {
			BIO_puts(srctx->err,
			    "cert_status: no AIA and no default responder URL\n");
			goto done;
		}
		host = srctx->host;
		path = srctx->path;
		port = srctx->port;
		use_ssl = srctx->use_ssl;
	}

	if ((inctx = X509_STORE_CTX_new()) == NULL)
		goto err;

	if (!X509_STORE_CTX_init(inctx,
	    SSL_CTX_get_cert_store(SSL_get_SSL_CTX(s)),
		NULL, NULL))
		goto err;
	if ((obj = X509_OBJECT_new()) == NULL)
		goto done;
	if (X509_STORE_get_by_subject(inctx, X509_LU_X509,
	    X509_get_issuer_name(x), obj) <= 0) {
		BIO_puts(err,
		    "cert_status: Can't retrieve issuer certificate.\n");
		X509_STORE_CTX_cleanup(inctx);
		goto done;
	}
	req = OCSP_REQUEST_new();
	if (!req)
		goto err;
	id = OCSP_cert_to_id(NULL, x, X509_OBJECT_get0_X509(obj));
	X509_OBJECT_free(obj);
	obj = NULL;
	X509_STORE_CTX_free(inctx);
	inctx = NULL;
	if (!id)
		goto err;
	if (!OCSP_request_add0_id(req, id))
		goto err;
	id = NULL;
	/* Add any extensions to the request */
	SSL_get_tlsext_status_exts(s, &exts);
	for (i = 0; i < sk_X509_EXTENSION_num(exts); i++) {
		X509_EXTENSION *ext = sk_X509_EXTENSION_value(exts, i);
		if (!OCSP_REQUEST_add_ext(req, ext, -1))
			goto err;
	}
	resp = process_responder(err, req, host, path, port, use_ssl, NULL,
	    srctx->timeout);
	if (!resp) {
		BIO_puts(err, "cert_status: error querying responder\n");
		goto done;
	}
	rspderlen = i2d_OCSP_RESPONSE(resp, &rspder);
	if (rspderlen <= 0)
		goto err;
	SSL_set_tlsext_status_ocsp_resp(s, rspder, rspderlen);
	if (srctx->verbose) {
		BIO_puts(err, "cert_status: ocsp response sent:\n");
		OCSP_RESPONSE_print(err, resp, 2);
	}
	ret = SSL_TLSEXT_ERR_OK;
 done:
	X509_STORE_CTX_free(inctx);
	X509_OBJECT_free(obj);
	if (ret != SSL_TLSEXT_ERR_OK)
		ERR_print_errors(err);
	if (aia) {
		free(host);
		free(path);
		free(port);
		X509_email_free(aia);
	}
	if (id)
		OCSP_CERTID_free(id);
	if (req)
		OCSP_REQUEST_free(req);
	if (resp)
		OCSP_RESPONSE_free(resp);
	return ret;
 err:
	ret = SSL_TLSEXT_ERR_ALERT_FATAL;
	goto done;
}

static int
alpn_cb(SSL *s, const unsigned char **out, unsigned char *outlen,
    const unsigned char *in, unsigned int inlen, void *arg)
{
	tlsextalpnctx *alpn_ctx = arg;

	if (!cfg.quiet) {
		/* We can assume that in is syntactically valid. */
		unsigned i;

		BIO_printf(bio_s_out,
		    "ALPN protocols advertised by the client: ");
		for (i = 0; i < inlen; ) {
			if (i)
				BIO_write(bio_s_out, ", ", 2);
			BIO_write(bio_s_out, &in[i + 1], in[i]);
			i += in[i] + 1;
		}
		BIO_write(bio_s_out, "\n", 1);
	}

	if (SSL_select_next_proto((unsigned char**)out, outlen, alpn_ctx->data,
	    alpn_ctx->len, in, inlen) != OPENSSL_NPN_NEGOTIATED)
		return (SSL_TLSEXT_ERR_NOACK);

	if (!cfg.quiet) {
		BIO_printf(bio_s_out, "ALPN protocols selected: ");
		BIO_write(bio_s_out, *out, *outlen);
		BIO_write(bio_s_out, "\n", 1);
	}

	return (SSL_TLSEXT_ERR_OK);
}
