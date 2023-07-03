/* $OpenBSD: s_client.c,v 1.62 2023/07/03 08:03:56 beck Exp $ */
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

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include "apps.h"

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/ocsp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "s_apps.h"
#include "timeouts.h"

/*#define SSL_HOST_NAME	"www.netscape.com" */
/*#define SSL_HOST_NAME	"193.118.187.102" */
#define SSL_HOST_NAME	"localhost"

 /*#define TEST_CERT "client.pem" *//* no default cert. */

#define BUFSIZZ 1024*8

static void sc_usage(void);
static void print_stuff(BIO *berr, SSL *con, int full);
static int ocsp_resp_cb(SSL *s, void *arg);
static int ssl_servername_cb(SSL *s, int *ad, void *arg);

enum {
	PROTO_OFF = 0,
	PROTO_SMTP,
	PROTO_LMTP,
	PROTO_POP3,
	PROTO_IMAP,
	PROTO_FTP,
	PROTO_XMPP,
};

/* This is a context that we pass to callbacks */
typedef struct tlsextctx_st {
	BIO *biodebug;
	int ack;
} tlsextctx;

static struct {
	int af;
	char *alpn_in;
	int bugs;
	char *CAfile;
	char *CApath;
	char *cert_file;
	int cert_format;
	char *cipher;
	unsigned int clr;
	char *connect;
	int crlf;
	int debug;
	int enable_timeouts;
	const char *errstr;
	char *groups_in;
	char *host;
	int ign_eof;
	char *key_file;
	int key_format;
	char *keymatexportlabel;
	int keymatexportlen;
	uint16_t max_version;
	uint16_t min_version;
	const SSL_METHOD *meth;
	int msg;
	int nbio;
	int nbio_test;
	int no_servername;
	char *npn_in;
	unsigned int off;
	char *passarg;
	int pause;
	int peekaboo;
	char *port;
	int prexit;
	char *proxy;
	int quiet;
	int reconnect;
	char *servername;
	char *sess_in;
	char *sess_out;
	int showcerts;
	int socket_type;
	long socket_mtu;
#ifndef OPENSSL_NO_SRTP
	char *srtp_profiles;
#endif
	int starttls_proto;
	int state;
	int status_req;
	int tlsextdebug;
	int verify;
	X509_VERIFY_PARAM *vpm;
	char *xmpphost;
} cfg;

static int
s_client_opt_keymatexportlen(char *arg)
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
s_client_opt_mtu(char *arg)
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

static int
s_client_opt_port(char *arg)
{
	if (*arg == '\0')
		return (1);

	cfg.port = arg;
	return (0);
}

#ifndef OPENSSL_NO_DTLS
static int
s_client_opt_protocol_version_dtls(void)
{
	cfg.meth = DTLS_client_method();
	cfg.socket_type = SOCK_DGRAM;
	return (0);
}
#endif

#ifndef OPENSSL_NO_DTLS1_2
static int
s_client_opt_protocol_version_dtls1_2(void)
{
	cfg.meth = DTLS_client_method();
	cfg.min_version = DTLS1_2_VERSION;
	cfg.max_version = DTLS1_2_VERSION;
	cfg.socket_type = SOCK_DGRAM;
	return (0);
}
#endif

static int
s_client_opt_protocol_version_tls1_2(void)
{
	cfg.min_version = TLS1_2_VERSION;
	cfg.max_version = TLS1_2_VERSION;
	return (0);
}

static int
s_client_opt_protocol_version_tls1_3(void)
{
	cfg.min_version = TLS1_3_VERSION;
	cfg.max_version = TLS1_3_VERSION;
	return (0);
}

static int
s_client_opt_quiet(void)
{
	cfg.quiet = 1;
	cfg.ign_eof = 1;
	return (0);
}

static int
s_client_opt_starttls(char *arg)
{
	if (strcmp(arg, "smtp") == 0)
		cfg.starttls_proto = PROTO_SMTP;
	else if (strcmp(arg, "lmtp") == 0)
		cfg.starttls_proto = PROTO_LMTP;
	else if (strcmp(arg, "pop3") == 0)
		cfg.starttls_proto = PROTO_POP3;
	else if (strcmp(arg, "imap") == 0)
		cfg.starttls_proto = PROTO_IMAP;
	else if (strcmp(arg, "ftp") == 0)
		cfg.starttls_proto = PROTO_FTP;
	else if (strcmp(arg, "xmpp") == 0)
		cfg.starttls_proto = PROTO_XMPP;
	else
		return (1);
	return (0);
}

static int
s_client_opt_verify(char *arg)
{
	cfg.verify = SSL_VERIFY_PEER;

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
s_client_opt_verify_param(int argc, char **argv, int *argsused)
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

static const struct option s_client_options[] = {
	{
		.name = "4",
		.desc = "Use IPv4 only",
		.type = OPTION_VALUE,
		.opt.value = &cfg.af,
		.value = AF_INET,
	},
	{
		.name = "6",
		.desc = "Use IPv6 only",
		.type = OPTION_VALUE,
		.opt.value = &cfg.af,
		.value = AF_INET6,
	},
	{
		.name = "alpn",
		.argname = "protocols",
		.desc = "Set the advertised protocols for ALPN"
			" (comma-separated list)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.alpn_in,
	},
	{
		.name = "bugs",
		.desc = "Enable various workarounds for buggy implementations",
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
		.desc = "Certificate file to use, PEM format assumed",
		.type = OPTION_ARG,
		.opt.arg = &cfg.cert_file,
	},
	{
		.name = "certform",
		.argname = "fmt",
		.desc = "Certificate format (PEM or DER) PEM default",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.cert_format,
	},
	{
		.name = "cipher",
		.argname = "cipherlist",
		.desc = "Preferred cipher to use (see 'openssl ciphers')",
		.type = OPTION_ARG,
		.opt.arg = &cfg.cipher,
	},
	{
		.name = "connect",
		.argname = "host:port",
		.desc = "Who to connect to (default is localhost:4433)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.connect,
	},
	{
		.name = "crlf",
		.desc = "Convert LF from terminal into CRLF",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.crlf,
	},
	{
		.name = "debug",
		.desc = "Print extensive debugging information",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.debug,
	},
#ifndef OPENSSL_NO_DTLS
	{
		.name = "dtls",
		.desc = "Use any version of DTLS",
		.type = OPTION_FUNC,
		.opt.func = s_client_opt_protocol_version_dtls,
	},
#endif
#ifndef OPENSSL_NO_DTLS1_2
	{
		.name = "dtls1_2",
		.desc = "Just use DTLSv1.2",
		.type = OPTION_FUNC,
		.opt.func = s_client_opt_protocol_version_dtls1_2,
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
		.name = "host",
		.argname = "host",
		.desc = "Use -connect instead",
		.type = OPTION_ARG,
		.opt.arg = &cfg.host,
	},
	{
		.name = "ign_eof",
		.desc = "Ignore input EOF (default when -quiet)",
		.type = OPTION_VALUE,
		.opt.value = &cfg.ign_eof,
		.value = 1,
	},
	{
		.name = "key",
		.argname = "file",
		.desc = "Private key file to use, if not, -cert file is used",
		.type = OPTION_ARG,
		.opt.arg = &cfg.key_file,
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
		.opt.argfunc = s_client_opt_keymatexportlen,
	},
	{
		.name = "legacy_renegotiation",
		.type = OPTION_DISCARD,
	},
	{
		.name = "legacy_server_connect",
		.desc = "Allow initial connection to servers that don't support RI",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.off,
		.value = SSL_OP_LEGACY_SERVER_CONNECT,
	},
	{
		.name = "msg",
		.desc = "Show all protocol messages with hex dump",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.msg,
	},
#ifndef OPENSSL_NO_DTLS
	{
		.name = "mtu",
		.argname = "mtu",
		.desc = "Set the link layer MTU on DTLS connections",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = s_client_opt_mtu,
	},
#endif
	{
		.name = "nbio",
		.desc = "Turn on non-blocking I/O",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.nbio,
	},
	{
		.name = "nbio_test",
		.desc = "Test non-blocking I/O",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.nbio_test,
	},
	{
		.name = "nextprotoneg",
		.argname = "protocols",
		.type = OPTION_ARG,
		.opt.arg = &cfg.npn_in, /* Ignored. */
	},
	{
		.name = "no_comp",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.off,
		.value = SSL_OP_NO_COMPRESSION,
	},
	{
		.name = "no_ign_eof",
		.desc = "Don't ignore input EOF",
		.type = OPTION_VALUE,
		.opt.value = &cfg.ign_eof,
		.value = 0,
	},
	{
		.name = "no_legacy_server_connect",
		.desc = "Disallow initial connection to servers that don't support RI",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.clr,
		.value = SSL_OP_LEGACY_SERVER_CONNECT,
	},
	{
		.name = "no_servername",
		.desc = "Do not send a Server Name Indication (SNI) extension",
		.type = OPTION_FLAG,
		.opt.value = &cfg.no_servername,
	},
	{
		.name = "no_ssl2",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.off,
		.value = SSL_OP_NO_SSLv2,
	},
	{
		.name = "no_ssl3",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.off,
		.value = SSL_OP_NO_SSLv3,
	},
	{
		.name = "no_ticket",
		.desc = "Disable use of RFC4507 session ticket support",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.off,
		.value = SSL_OP_NO_TICKET,
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
		.desc = "Disable the use of TLSv1.2",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.off,
		.value = SSL_OP_NO_TLSv1_2,
	},
	{
		.name = "no_tls1_3",
		.desc = "Disable the use of TLSv1.3",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.off,
		.value = SSL_OP_NO_TLSv1_3,
	},
	{
		.name = "noservername",
		.type = OPTION_FLAG,
		.opt.value = &cfg.no_servername,
	},
	{
		.name = "pass",
		.argname = "arg",
		.desc = "Private key file pass phrase source",
		.type = OPTION_ARG,
		.opt.arg = &cfg.passarg,
	},
	{
		.name = "pause",
		.desc = "Pause 1 second between each read and write call",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.pause,
	},
	{
		.name = "peekaboo",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.peekaboo,
	},
	{
		.name = "port",
		.argname = "port",
		.desc = "Use -connect instead",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = s_client_opt_port,
	},
	{
		.name = "prexit",
		.desc = "Print session information when the program exits",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.prexit,
	},
	{
		.name = "proxy",
		.argname = "host:port",
		.desc = "Connect to http proxy",
		.type = OPTION_ARG,
		.opt.arg = &cfg.proxy,
	},
	{
		.name = "quiet",
		.desc = "Inhibit printing of session and certificate info",
		.type = OPTION_FUNC,
		.opt.func = s_client_opt_quiet,
	},
	{
		.name = "reconnect",
		.desc = "Drop and re-make the connection with the same Session-ID",
		.type = OPTION_VALUE,
		.opt.value = &cfg.reconnect,
		.value = 5,
	},
	{
		.name = "servername",
		.argname = "name",
		.desc = "Set TLS extension servername in ClientHello (SNI)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.servername,
	},
	{
		.name = "serverpref",
		.desc = "Use server's cipher preferences",
		.type = OPTION_VALUE_OR,
		.opt.value = &cfg.off,
		.value = SSL_OP_CIPHER_SERVER_PREFERENCE,
	},
	{
		.name = "sess_in",
		.argname = "file",
		.desc = "File to read TLS session from",
		.type = OPTION_ARG,
		.opt.arg = &cfg.sess_in,
	},
	{
		.name = "sess_out",
		.argname = "file",
		.desc = "File to write TLS session to",
		.type = OPTION_ARG,
		.opt.arg = &cfg.sess_out,
	},
	{
		.name = "showcerts",
		.desc = "Show all server certificates in the chain",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.showcerts,
	},
	{
		.name = "starttls",
		.argname = "protocol",
		.desc = "Use the STARTTLS command before starting TLS,\n"
		        "smtp, lmtp, pop3, imap, ftp and xmpp are supported.",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = s_client_opt_starttls,
	},
	{
		.name = "state",
		.desc = "Print the TLS session states",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.state,
	},
	{
		.name = "status",
		.desc = "Send a certificate status request to the server (OCSP)",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.status_req,
	},
#ifndef OPENSSL_NO_DTLS
	{
		.name = "timeout",
		.desc = "Enable send/receive timeout on DTLS connections",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.enable_timeouts,
	},
#endif
	{
		.name = "tls1_2",
		.desc = "Just use TLSv1.2",
		.type = OPTION_FUNC,
		.opt.func = s_client_opt_protocol_version_tls1_2,
	},
	{
		.name = "tls1_3",
		.desc = "Just use TLSv1.3",
		.type = OPTION_FUNC,
		.opt.func = s_client_opt_protocol_version_tls1_3,
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
		.desc = "Offer SRTP key management with a colon-separated profiles",
		.type = OPTION_ARG,
		.opt.arg = &cfg.srtp_profiles,
	},
#endif
	{
		.name = "verify",
		.argname = "depth",
		.desc = "Turn on peer certificate verification, with a max of depth",
		.type = OPTION_ARG_FUNC,
		.opt.argfunc = s_client_opt_verify,
	},
	{
		.name = "verify_return_error",
		.desc = "Return verification error",
		.type = OPTION_FLAG,
		.opt.flag = &verify_return_error,
	},
	{
		.name = "xmpphost",
		.argname = "host",
		.desc = "Connect to this virtual host on the xmpp server",
		.type = OPTION_ARG,
		.opt.arg = &cfg.xmpphost,
	},
	{
		.name = NULL,
		.desc = "",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = s_client_opt_verify_param,
	},
	{ NULL },
};

static void
sc_usage(void)
{
	fprintf(stderr, "usage: s_client "
	    "[-4 | -6] [-alpn protocols] [-bugs] [-CAfile file]\n"
	    "    [-CApath directory] [-cert file] [-certform der | pem] [-check_ss_sig]\n"
	    "    [-cipher cipherlist] [-connect host[:port]] [-crl_check]\n"
	    "    [-crl_check_all] [-crlf] [-debug] [-dtls] [-dtls1_2] [-extended_crl]\n"
	    "    [-groups list] [-host host] [-ign_eof] [-ignore_critical]\n"
	    "    [-issuer_checks] [-key keyfile] [-keyform der | pem]\n"
	    "    [-keymatexport label] [-keymatexportlen len] [-legacy_server_connect]\n"
	    "    [-msg] [-mtu mtu] [-nbio] [-nbio_test] [-no_comp] [-no_ign_eof]\n"
	    "    [-no_legacy_server_connect] [-no_ticket] \n"
	    "    [-no_tls1_2] [-no_tls1_3] [-pass arg] [-pause] [-policy_check]\n"
	    "    [-port port] [-prexit] [-proxy host:port] [-quiet] [-reconnect]\n"
	    "    [-servername name] [-serverpref] [-sess_in file] [-sess_out file]\n"
	    "    [-showcerts] [-starttls protocol] [-state] [-status] [-timeout]\n"
	    "    [-tls1_2] [-tls1_3] [-tlsextdebug]\n"
	    "    [-use_srtp profiles] [-verify depth] [-verify_return_error]\n"
	    "    [-x509_strict] [-xmpphost host]\n");
	fprintf(stderr, "\n");
	options_usage(s_client_options);
	fprintf(stderr, "\n");
}

int
s_client_main(int argc, char **argv)
{
	SSL *con = NULL;
	int s, k, p = 0, pending = 0;
	char *cbuf = NULL, *sbuf = NULL, *mbuf = NULL, *pbuf = NULL;
	int cbuf_len, cbuf_off;
	int sbuf_len, sbuf_off;
	int full_log = 1;
	const char *servername;
	char *pass = NULL;
	X509 *cert = NULL;
	EVP_PKEY *key = NULL;
	int badop = 0;
	int write_tty, read_tty, write_ssl, read_ssl, tty_on, ssl_pending;
	SSL_CTX *ctx = NULL;
	int ret = 1, in_init = 1, i;
	BIO *bio_c_out = NULL;
	BIO *sbio;
	int mbuf_len = 0;
	struct timeval timeout;
	tlsextctx tlsextcbp = {NULL, 0};
	struct sockaddr_storage peer;
	int peerlen = sizeof(peer);

	if (pledge("stdio cpath wpath rpath inet dns tty", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.af = AF_UNSPEC;
	cfg.cert_format = FORMAT_PEM;
	cfg.host = SSL_HOST_NAME;
	cfg.key_format = FORMAT_PEM;
	cfg.keymatexportlen = 20;
	cfg.meth = TLS_client_method();
	cfg.port = PORT_STR;
	cfg.socket_type = SOCK_STREAM;
	cfg.starttls_proto = PROTO_OFF;
	cfg.verify = SSL_VERIFY_NONE;

	if (((cbuf = malloc(BUFSIZZ)) == NULL) ||
	    ((sbuf = malloc(BUFSIZZ)) == NULL) ||
	    ((pbuf = malloc(BUFSIZZ)) == NULL) ||
	    ((mbuf = malloc(BUFSIZZ + 1)) == NULL)) {	/* NUL byte */
		BIO_printf(bio_err, "out of memory\n");
		goto end;
	}
	verify_depth = 0;

	if (options_parse(argc, argv, s_client_options, NULL, NULL) != 0) {
		badop = 1;
		goto bad;
	}
	if (cfg.proxy != NULL) {
		if (!extract_host_port(cfg.proxy,
		    &cfg.host, NULL, &cfg.port))
			goto bad;
		if (cfg.connect == NULL)
			cfg.connect = SSL_HOST_NAME;
	} else if (cfg.connect != NULL) {
		if (!extract_host_port(cfg.connect,
		    &cfg.host, NULL, &cfg.port))
			goto bad;
	}
	if (badop) {
 bad:
		if (cfg.errstr == NULL)
			sc_usage();
		goto end;
	}

	if (!app_passwd(bio_err, cfg.passarg, NULL, &pass, NULL)) {
		BIO_printf(bio_err, "Error getting password\n");
		goto end;
	}
	if (cfg.key_file == NULL)
		cfg.key_file = cfg.cert_file;


	if (cfg.key_file) {

		key = load_key(bio_err, cfg.key_file,
		    cfg.key_format, 0, pass,
		    "client certificate private key file");
		if (!key) {
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	if (cfg.cert_file) {
		cert = load_cert(bio_err, cfg.cert_file,
		    cfg.cert_format,
		    NULL, "client certificate file");

		if (!cert) {
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	if (cfg.quiet && !cfg.debug &&
	    !cfg.msg) {
		if ((bio_c_out = BIO_new(BIO_s_null())) == NULL)
			goto end;
	} else {
		if ((bio_c_out = BIO_new_fp(stdout, BIO_NOCLOSE)) == NULL)
			goto end;
	}

	ctx = SSL_CTX_new(cfg.meth);
	if (ctx == NULL) {
		ERR_print_errors(bio_err);
		goto end;
	}

	SSL_CTX_clear_mode(ctx, SSL_MODE_AUTO_RETRY);

	if (cfg.vpm)
		SSL_CTX_set1_param(ctx, cfg.vpm);

	if (!SSL_CTX_set_min_proto_version(ctx, cfg.min_version))
		goto end;
	if (!SSL_CTX_set_max_proto_version(ctx, cfg.max_version))
		goto end;

#ifndef OPENSSL_NO_SRTP
	if (cfg.srtp_profiles != NULL)
		SSL_CTX_set_tlsext_use_srtp(ctx, cfg.srtp_profiles);
#endif
	if (cfg.bugs)
		SSL_CTX_set_options(ctx, SSL_OP_ALL | cfg.off);
	else
		SSL_CTX_set_options(ctx, cfg.off);

	if (cfg.clr)
		SSL_CTX_clear_options(ctx, cfg.clr);

	if (cfg.alpn_in) {
		unsigned short alpn_len;
		unsigned char *alpn;

		alpn = next_protos_parse(&alpn_len, cfg.alpn_in);
		if (alpn == NULL) {
			BIO_printf(bio_err, "Error parsing -alpn argument\n");
			goto end;
		}
		SSL_CTX_set_alpn_protos(ctx, alpn, alpn_len);
		free(alpn);
	}
	if (cfg.groups_in != NULL) {
		if (SSL_CTX_set1_groups_list(ctx, cfg.groups_in) != 1) {
			BIO_printf(bio_err, "Failed to set groups '%s'\n",
			    cfg.groups_in);
			goto end;
		}
	}

	if (cfg.state)
		SSL_CTX_set_info_callback(ctx, apps_ssl_info_callback);
	if (cfg.cipher != NULL)
		if (!SSL_CTX_set_cipher_list(ctx, cfg.cipher)) {
			BIO_printf(bio_err, "error setting cipher list\n");
			ERR_print_errors(bio_err);
			goto end;
		}

	SSL_CTX_set_verify(ctx, cfg.verify, verify_callback);
	if (!set_cert_key_stuff(ctx, cert, key))
		goto end;

	if ((cfg.CAfile || cfg.CApath)
	    && !SSL_CTX_load_verify_locations(ctx, cfg.CAfile,
	    cfg.CApath))
		ERR_print_errors(bio_err);

	if (!SSL_CTX_set_default_verify_paths(ctx))
		ERR_print_errors(bio_err);

	con = SSL_new(ctx);
	if (cfg.sess_in) {
		SSL_SESSION *sess;
		BIO *stmp = BIO_new_file(cfg.sess_in, "r");
		if (!stmp) {
			BIO_printf(bio_err, "Can't open session file %s\n",
			    cfg.sess_in);
			ERR_print_errors(bio_err);
			goto end;
		}
		sess = PEM_read_bio_SSL_SESSION(stmp, NULL, 0, NULL);
		BIO_free(stmp);
		if (!sess) {
			BIO_printf(bio_err, "Can't open session file %s\n",
			    cfg.sess_in);
			ERR_print_errors(bio_err);
			goto end;
		}
		SSL_set_session(con, sess);
		SSL_SESSION_free(sess);
	}

	/* Attempt to opportunistically use the host name for SNI. */
	servername = cfg.servername;
	if (servername == NULL)
		servername = cfg.host;

	if (!cfg.no_servername && servername != NULL &&
	    !SSL_set_tlsext_host_name(con, servername)) {
		long ssl_err = ERR_peek_error();

		if (cfg.servername != NULL ||
		    ERR_GET_LIB(ssl_err) != ERR_LIB_SSL ||
		    ERR_GET_REASON(ssl_err) != SSL_R_SSL3_EXT_INVALID_SERVERNAME) {
			BIO_printf(bio_err,
			    "Unable to set TLS servername extension.\n");
			ERR_print_errors(bio_err);
			goto end;
		}
		servername = NULL;
		ERR_clear_error();
	}
	if (!cfg.no_servername && servername != NULL) {
		tlsextcbp.biodebug = bio_err;
		SSL_CTX_set_tlsext_servername_callback(ctx, ssl_servername_cb);
		SSL_CTX_set_tlsext_servername_arg(ctx, &tlsextcbp);
	}

 re_start:

	if (init_client(&s, cfg.host, cfg.port,
	    cfg.socket_type, cfg.af) == 0) {
		BIO_printf(bio_err, "connect:errno=%d\n", errno);
		goto end;
	}
	BIO_printf(bio_c_out, "CONNECTED(%08X)\n", s);

	if (cfg.nbio) {
		if (!cfg.quiet)
			BIO_printf(bio_c_out, "turning on non blocking io\n");
		if (!BIO_socket_nbio(s, 1)) {
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	if (cfg.pause & 0x01)
		SSL_set_debug(con, 1);

	if (SSL_is_dtls(con)) {
		sbio = BIO_new_dgram(s, BIO_NOCLOSE);
		if (getsockname(s, (struct sockaddr *)&peer,
		    (void *)&peerlen) == -1) {
			BIO_printf(bio_err, "getsockname:errno=%d\n",
			    errno);
			shutdown(s, SHUT_RD);
			close(s);
			goto end;
		}
		(void) BIO_ctrl_set_connected(sbio, 1, &peer);

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
	} else
		sbio = BIO_new_socket(s, BIO_NOCLOSE);

	if (cfg.nbio_test) {
		BIO *test;

		test = BIO_new(BIO_f_nbio_test());
		sbio = BIO_push(test, sbio);
	}
	if (cfg.debug) {
		SSL_set_debug(con, 1);
		BIO_set_callback(sbio, bio_dump_callback);
		BIO_set_callback_arg(sbio, (char *) bio_c_out);
	}
	if (cfg.msg) {
		SSL_set_msg_callback(con, msg_cb);
		SSL_set_msg_callback_arg(con, bio_c_out);
	}
	if (cfg.tlsextdebug) {
		SSL_set_tlsext_debug_callback(con, tlsext_cb);
		SSL_set_tlsext_debug_arg(con, bio_c_out);
	}
	if (cfg.status_req) {
		SSL_set_tlsext_status_type(con, TLSEXT_STATUSTYPE_ocsp);
		SSL_CTX_set_tlsext_status_cb(ctx, ocsp_resp_cb);
		SSL_CTX_set_tlsext_status_arg(ctx, bio_c_out);
	}

	SSL_set_bio(con, sbio, sbio);
	SSL_set_connect_state(con);

	/* ok, lets connect */
	read_tty = 1;
	write_tty = 0;
	tty_on = 0;
	read_ssl = 1;
	write_ssl = 1;

	cbuf_len = 0;
	cbuf_off = 0;
	sbuf_len = 0;
	sbuf_off = 0;

	/* This is an ugly hack that does a lot of assumptions */
	/*
	 * We do have to handle multi-line responses which may come in a
	 * single packet or not. We therefore have to use BIO_gets() which
	 * does need a buffering BIO. So during the initial chitchat we do
	 * push a buffering BIO into the chain that is removed again later on
	 * to not disturb the rest of the s_client operation.
	 */
	if (cfg.starttls_proto == PROTO_SMTP ||
	    cfg.starttls_proto == PROTO_LMTP) {
		int foundit = 0;
		BIO *fbio = BIO_new(BIO_f_buffer());
		BIO_push(fbio, sbio);
		/* wait for multi-line response to end from SMTP */
		do {
			mbuf_len = BIO_gets(fbio, mbuf, BUFSIZZ);
		}
		while (mbuf_len > 3 && mbuf[3] == '-');
		/* STARTTLS command requires EHLO... */
		BIO_printf(fbio, "%cHLO openssl.client.net\r\n",
		    cfg.starttls_proto == PROTO_SMTP ? 'E' : 'L');
		(void) BIO_flush(fbio);
		/* wait for multi-line response to end EHLO SMTP response */
		do {
			mbuf_len = BIO_gets(fbio, mbuf, BUFSIZZ);
			if (strstr(mbuf, "STARTTLS"))
				foundit = 1;
		}
		while (mbuf_len > 3 && mbuf[3] == '-');
		(void) BIO_flush(fbio);
		BIO_pop(fbio);
		BIO_free(fbio);
		if (!foundit)
			BIO_printf(bio_err,
			    "didn't find starttls in server response,"
			    " try anyway...\n");
		BIO_printf(sbio, "STARTTLS\r\n");
		BIO_read(sbio, sbuf, BUFSIZZ);
	} else if (cfg.starttls_proto == PROTO_POP3) {
		mbuf_len = BIO_read(sbio, mbuf, BUFSIZZ);
		if (mbuf_len == -1) {
			BIO_printf(bio_err, "BIO_read failed\n");
			goto end;
		}
		BIO_printf(sbio, "STLS\r\n");
		BIO_read(sbio, sbuf, BUFSIZZ);
	} else if (cfg.starttls_proto == PROTO_IMAP) {
		int foundit = 0;
		BIO *fbio = BIO_new(BIO_f_buffer());
		BIO_push(fbio, sbio);
		BIO_gets(fbio, mbuf, BUFSIZZ);
		/* STARTTLS command requires CAPABILITY... */
		BIO_printf(fbio, ". CAPABILITY\r\n");
		(void) BIO_flush(fbio);
		/* wait for multi-line CAPABILITY response */
		do {
			mbuf_len = BIO_gets(fbio, mbuf, BUFSIZZ);
			if (strstr(mbuf, "STARTTLS"))
				foundit = 1;
		}
		while (mbuf_len > 3 && mbuf[0] != '.');
		(void) BIO_flush(fbio);
		BIO_pop(fbio);
		BIO_free(fbio);
		if (!foundit)
			BIO_printf(bio_err,
			    "didn't find STARTTLS in server response,"
			    " try anyway...\n");
		BIO_printf(sbio, ". STARTTLS\r\n");
		BIO_read(sbio, sbuf, BUFSIZZ);
	} else if (cfg.starttls_proto == PROTO_FTP) {
		BIO *fbio = BIO_new(BIO_f_buffer());
		BIO_push(fbio, sbio);
		/* wait for multi-line response to end from FTP */
		do {
			mbuf_len = BIO_gets(fbio, mbuf, BUFSIZZ);
		}
		while (mbuf_len > 3 && mbuf[3] == '-');
		(void) BIO_flush(fbio);
		BIO_pop(fbio);
		BIO_free(fbio);
		BIO_printf(sbio, "AUTH TLS\r\n");
		BIO_read(sbio, sbuf, BUFSIZZ);
	} else if (cfg.starttls_proto == PROTO_XMPP) {
		int seen = 0;
		BIO_printf(sbio, "<stream:stream "
		    "xmlns:stream='http://etherx.jabber.org/streams' "
		    "xmlns='jabber:client' to='%s' version='1.0'>",
		    cfg.xmpphost ?
		    cfg.xmpphost : cfg.host);
		seen = BIO_read(sbio, mbuf, BUFSIZZ);

		if (seen <= 0)
			goto shut;

		mbuf[seen] = 0;
		while (!strstr(mbuf, "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'") &&
		       !strstr(mbuf, "<starttls xmlns=\"urn:ietf:params:xml:ns:xmpp-tls\"")) {
			seen = BIO_read(sbio, mbuf, BUFSIZZ);

			if (seen <= 0)
				goto shut;

			mbuf[seen] = 0;
		}
		BIO_printf(sbio,
		    "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>");
		seen = BIO_read(sbio, sbuf, BUFSIZZ);
		sbuf[seen] = 0;
		if (!strstr(sbuf, "<proceed"))
			goto shut;
		mbuf[0] = 0;
	} else if (cfg.proxy != NULL) {
		BIO_printf(sbio, "CONNECT %s HTTP/1.0\r\n\r\n",
		    cfg.connect);
		mbuf_len = BIO_read(sbio, mbuf, BUFSIZZ);
		if (mbuf_len == -1) {
			BIO_printf(bio_err, "BIO_read failed\n");
			goto end;
		}
	}
	for (;;) {
		struct pollfd pfd[3];	/* stdin, stdout, socket */
		int ptimeout = -1;

		if (SSL_is_dtls(con) && DTLSv1_get_timeout(con, &timeout))
			ptimeout = timeout.tv_sec * 1000 +
			    timeout.tv_usec / 1000;

		if (SSL_in_init(con) && !SSL_total_renegotiations(con)) {
			in_init = 1;
			tty_on = 0;
		} else {
			tty_on = 1;
			if (in_init) {
				in_init = 0;
				if (cfg.sess_out) {
					BIO *stmp = BIO_new_file(
					    cfg.sess_out, "w");
					if (stmp) {
						PEM_write_bio_SSL_SESSION(stmp,
						    SSL_get_session(con));
						BIO_free(stmp);
					} else
						BIO_printf(bio_err,
						    "Error writing session file %s\n",
						    cfg.sess_out);
				}
				print_stuff(bio_c_out, con, full_log);
				if (full_log > 0)
					full_log--;

				if (cfg.starttls_proto) {
					BIO_write(bio_err, mbuf, mbuf_len);
					/* We don't need to know any more */
					cfg.starttls_proto = PROTO_OFF;
				}
				if (cfg.reconnect) {
					cfg.reconnect--;
					BIO_printf(bio_c_out,
					    "drop connection and then reconnect\n");
					SSL_shutdown(con);
					SSL_set_connect_state(con);
					shutdown(SSL_get_fd(con), SHUT_RD);
					close(SSL_get_fd(con));
					goto re_start;
				}
			}
		}

		ssl_pending = read_ssl && SSL_pending(con);

		pfd[0].fd = -1;
		pfd[1].fd = -1;
		if (!ssl_pending) {
			if (tty_on) {
				if (read_tty) {
					pfd[0].fd = fileno(stdin);
					pfd[0].events = POLLIN;
				}
				if (write_tty) {
					pfd[1].fd = fileno(stdout);
					pfd[1].events = POLLOUT;
				}
			}

			pfd[2].fd = SSL_get_fd(con);
			pfd[2].events = 0;
			if (read_ssl)
				pfd[2].events |= POLLIN;
			if (write_ssl)
				pfd[2].events |= POLLOUT;

/*			printf("mode tty(%d %d%d) ssl(%d%d)\n",
				tty_on,read_tty,write_tty,read_ssl,write_ssl);*/

			i = poll(pfd, 3, ptimeout);
			if (i == -1) {
				BIO_printf(bio_err, "bad select %d\n",
				    errno);
				goto shut;
				/* goto end; */
			}
		}
		if (SSL_is_dtls(con) &&
		    DTLSv1_handle_timeout(con) > 0)
			BIO_printf(bio_err, "TIMEOUT occured\n");
		if (!ssl_pending &&
		    (pfd[2].revents & (POLLOUT|POLLERR|POLLNVAL))) {
			if (pfd[2].revents & (POLLERR|POLLNVAL)) {
				BIO_printf(bio_err, "poll error");
				goto shut;
			}
			k = SSL_write(con, &(cbuf[cbuf_off]),
			    (unsigned int) cbuf_len);
			switch (SSL_get_error(con, k)) {
			case SSL_ERROR_NONE:
				cbuf_off += k;
				cbuf_len -= k;
				if (k <= 0)
					goto end;
				/* we have done a  write(con,NULL,0); */
				if (cbuf_len <= 0) {
					read_tty = 1;
					write_ssl = 0;
				} else {	/* if (cbuf_len > 0) */
					read_tty = 0;
					write_ssl = 1;
				}
				break;
			case SSL_ERROR_WANT_WRITE:
				BIO_printf(bio_c_out, "write W BLOCK\n");
				write_ssl = 1;
				read_tty = 0;
				break;
			case SSL_ERROR_WANT_READ:
				BIO_printf(bio_c_out, "write R BLOCK\n");
				write_tty = 0;
				read_ssl = 1;
				write_ssl = 0;
				break;
			case SSL_ERROR_WANT_X509_LOOKUP:
				BIO_printf(bio_c_out, "write X BLOCK\n");
				break;
			case SSL_ERROR_ZERO_RETURN:
				if (cbuf_len != 0) {
					BIO_printf(bio_c_out, "shutdown\n");
					ret = 0;
					goto shut;
				} else {
					read_tty = 1;
					write_ssl = 0;
					break;
				}

			case SSL_ERROR_SYSCALL:
				if ((k != 0) || (cbuf_len != 0)) {
					BIO_printf(bio_err, "write:errno=%d\n",
					    errno);
					goto shut;
				} else {
					read_tty = 1;
					write_ssl = 0;
				}
				break;
			case SSL_ERROR_SSL:
				ERR_print_errors(bio_err);
				goto shut;
			}
		} else if (!ssl_pending &&
		    (pfd[1].revents & (POLLOUT|POLLERR|POLLNVAL))) {
			if (pfd[1].revents & (POLLERR|POLLNVAL)) {
				BIO_printf(bio_err, "poll error");
				goto shut;
			}
			i = write(fileno(stdout), &(sbuf[sbuf_off]), sbuf_len);

			if (i <= 0) {
				BIO_printf(bio_c_out, "DONE\n");
				ret = 0;
				goto shut;
				/* goto end; */
			}
			sbuf_len -= i;
			sbuf_off += i;
			if (sbuf_len <= 0) {
				read_ssl = 1;
				write_tty = 0;
			}
		} else if (ssl_pending || (pfd[2].revents & (POLLIN|POLLHUP))) {
#ifdef RENEG
			{
				static int iiii;
				if (++iiii == 52) {
					SSL_renegotiate(con);
					iiii = 0;
				}
			}
#endif
			if (cfg.peekaboo) {
				k = p = SSL_peek(con, pbuf, 1024 /* BUFSIZZ */ );
				pending = SSL_pending(con);
				if (SSL_get_error(con, p) == SSL_ERROR_NONE) {
					if (p <= 0)
						goto end;

					k = SSL_read(con, sbuf, p);
				}
			} else {
				k = SSL_read(con, sbuf, 1024 /* BUFSIZZ */ );
			}

			switch (SSL_get_error(con, k)) {
			case SSL_ERROR_NONE:
				if (k <= 0)
					goto end;
				sbuf_off = 0;
				sbuf_len = k;
				if (cfg.peekaboo) {
					if (p != pending) {
						ret = -1;
						BIO_printf(bio_err,
						    "peeked %d but pending %d!\n",
						    p, pending);
						goto shut;
					}
					if (k < p) {
						ret = -1;
						BIO_printf(bio_err,
						    "read less than peek!\n");
						goto shut;
					}
					if (p > 0 &&
					    (memcmp(sbuf, pbuf, p) != 0)) {
						ret = -1;
						BIO_printf(bio_err,
						    "peek of %d different from read of %d!\n",
						    p, k);
						goto shut;
					}
				}
				read_ssl = 0;
				write_tty = 1;
				break;
			case SSL_ERROR_WANT_WRITE:
				BIO_printf(bio_c_out, "read W BLOCK\n");
				write_ssl = 1;
				read_tty = 0;
				break;
			case SSL_ERROR_WANT_READ:
				BIO_printf(bio_c_out, "read R BLOCK\n");
				write_tty = 0;
				read_ssl = 1;
				if ((read_tty == 0) && (write_ssl == 0))
					write_ssl = 1;
				break;
			case SSL_ERROR_WANT_X509_LOOKUP:
				BIO_printf(bio_c_out, "read X BLOCK\n");
				break;
			case SSL_ERROR_SYSCALL:
				ret = errno;
				BIO_printf(bio_err, "read:errno=%d\n", ret);
				goto shut;
			case SSL_ERROR_ZERO_RETURN:
				BIO_printf(bio_c_out, "closed\n");
				ret = 0;
				goto shut;
			case SSL_ERROR_SSL:
				ERR_print_errors(bio_err);
				goto shut;
				/* break; */
			}
		} else if (pfd[0].revents) {
			if (pfd[0].revents & (POLLERR|POLLNVAL)) {
				BIO_printf(bio_err, "poll error");
				goto shut;
			}
			if (cfg.crlf) {
				int j, lf_num;

				i = read(fileno(stdin), cbuf, BUFSIZZ / 2);
				lf_num = 0;
				/* both loops are skipped when i <= 0 */
				for (j = 0; j < i; j++)
					if (cbuf[j] == '\n')
						lf_num++;
				for (j = i - 1; j >= 0; j--) {
					cbuf[j + lf_num] = cbuf[j];
					if (cbuf[j] == '\n') {
						lf_num--;
						i++;
						cbuf[j + lf_num] = '\r';
					}
				}
				assert(lf_num == 0);
			} else
				i = read(fileno(stdin), cbuf, BUFSIZZ);

			if ((!cfg.ign_eof) &&
			    ((i <= 0) || (cbuf[0] == 'Q'))) {
				BIO_printf(bio_err, "DONE\n");
				ret = 0;
				goto shut;
			}
			if ((!cfg.ign_eof) && (cbuf[0] == 'R')) {
				BIO_printf(bio_err, "RENEGOTIATING\n");
				SSL_renegotiate(con);
				cbuf_len = 0;
			} else {
				cbuf_len = i;
				cbuf_off = 0;
			}

			write_ssl = 1;
			read_tty = 0;
		}
	}

	ret = 0;
 shut:
	if (in_init)
		print_stuff(bio_c_out, con, full_log);
	SSL_shutdown(con);
	shutdown(SSL_get_fd(con), SHUT_RD);
	close(SSL_get_fd(con));
 end:
	if (con != NULL) {
		if (cfg.prexit != 0)
			print_stuff(bio_c_out, con, 1);
		SSL_free(con);
	}
	SSL_CTX_free(ctx);
	X509_free(cert);
	EVP_PKEY_free(key);
	free(pass);
	X509_VERIFY_PARAM_free(cfg.vpm);
	freezero(cbuf, BUFSIZZ);
	freezero(sbuf, BUFSIZZ);
	freezero(pbuf, BUFSIZZ);
	freezero(mbuf, BUFSIZZ);
	BIO_free(bio_c_out);

	return (ret);
}

static void
print_stuff(BIO *bio, SSL *s, int full)
{
	X509 *peer = NULL;
	char *p;
	static const char *space = "                ";
	char buf[BUFSIZ];
	STACK_OF(X509) *sk;
	STACK_OF(X509_NAME) *sk2;
	const SSL_CIPHER *c;
	X509_NAME *xn;
	int j, i;
	unsigned char *exportedkeymat;

	if (full) {
		int got_a_chain = 0;

		sk = SSL_get_peer_cert_chain(s);
		if (sk != NULL) {
			got_a_chain = 1;	/* we don't have it for SSL2
						 * (yet) */

			BIO_printf(bio, "---\nCertificate chain\n");
			for (i = 0; i < sk_X509_num(sk); i++) {
				X509_NAME_oneline(X509_get_subject_name(
					sk_X509_value(sk, i)), buf, sizeof buf);
				BIO_printf(bio, "%2d s:%s\n", i, buf);
				X509_NAME_oneline(X509_get_issuer_name(
					sk_X509_value(sk, i)), buf, sizeof buf);
				BIO_printf(bio, "   i:%s\n", buf);
				if (cfg.showcerts)
					PEM_write_bio_X509(bio,
					    sk_X509_value(sk, i));
			}
		}
		BIO_printf(bio, "---\n");
		peer = SSL_get_peer_certificate(s);
		if (peer != NULL) {
			BIO_printf(bio, "Server certificate\n");
			if (!(cfg.showcerts && got_a_chain)) {
				/* Redundant if we showed the whole chain */
				PEM_write_bio_X509(bio, peer);
			}
			X509_NAME_oneline(X509_get_subject_name(peer),
			    buf, sizeof buf);
			BIO_printf(bio, "subject=%s\n", buf);
			X509_NAME_oneline(X509_get_issuer_name(peer),
			    buf, sizeof buf);
			BIO_printf(bio, "issuer=%s\n", buf);
		} else
			BIO_printf(bio, "no peer certificate available\n");

		sk2 = SSL_get_client_CA_list(s);
		if ((sk2 != NULL) && (sk_X509_NAME_num(sk2) > 0)) {
			BIO_printf(bio,
			    "---\nAcceptable client certificate CA names\n");
			for (i = 0; i < sk_X509_NAME_num(sk2); i++) {
				xn = sk_X509_NAME_value(sk2, i);
				X509_NAME_oneline(xn, buf, sizeof(buf));
				BIO_write(bio, buf, strlen(buf));
				BIO_write(bio, "\n", 1);
			}
		} else {
			BIO_printf(bio,
			    "---\nNo client certificate CA names sent\n");
		}
		p = SSL_get_shared_ciphers(s, buf, sizeof buf);
		if (p != NULL) {
			/*
			 * This works only for SSL 2.  In later protocol
			 * versions, the client does not know what other
			 * ciphers (in addition to the one to be used in the
			 * current connection) the server supports.
			 */

			BIO_printf(bio,
			    "---\nCiphers common between both SSL endpoints:\n");
			j = i = 0;
			while (*p) {
				if (*p == ':') {
					BIO_write(bio, space, 15 - j % 25);
					i++;
					j = 0;
					BIO_write(bio,
					    ((i % 3) ? " " : "\n"), 1);
				} else {
					BIO_write(bio, p, 1);
					j++;
				}
				p++;
			}
			BIO_write(bio, "\n", 1);
		}

		ssl_print_tmp_key(bio, s);

		BIO_printf(bio,
		    "---\nSSL handshake has read %ld bytes and written %ld bytes\n",
		    BIO_number_read(SSL_get_rbio(s)),
		    BIO_number_written(SSL_get_wbio(s)));
	}
	BIO_printf(bio, (SSL_cache_hit(s) ? "---\nReused, " : "---\nNew, "));
	c = SSL_get_current_cipher(s);
	BIO_printf(bio, "%s, Cipher is %s\n",
	    SSL_CIPHER_get_version(c),
	    SSL_CIPHER_get_name(c));
	if (peer != NULL) {
		EVP_PKEY *pktmp;

		pktmp = X509_get0_pubkey(peer);
		BIO_printf(bio, "Server public key is %d bit\n",
		    EVP_PKEY_bits(pktmp));
	}
	BIO_printf(bio, "Secure Renegotiation IS%s supported\n",
	    SSL_get_secure_renegotiation_support(s) ? "" : " NOT");

	/* Compression is not supported and will always be none. */
	BIO_printf(bio, "Compression: NONE\n");
	BIO_printf(bio, "Expansion: NONE\n");

#ifdef SSL_DEBUG
	{
		/* Print out local port of connection: useful for debugging */
		int sock;
		struct sockaddr_in ladd;
		socklen_t ladd_size = sizeof(ladd);
		sock = SSL_get_fd(s);
		getsockname(sock, (struct sockaddr *) & ladd, &ladd_size);
		BIO_printf(bio, "LOCAL PORT is %u\n",
		    ntohs(ladd.sin_port));
	}
#endif

	{
		const unsigned char *proto;
		unsigned int proto_len;
		SSL_get0_alpn_selected(s, &proto, &proto_len);
		if (proto_len > 0) {
			BIO_printf(bio, "ALPN protocol: ");
			BIO_write(bio, proto, proto_len);
			BIO_write(bio, "\n", 1);
		} else
			BIO_printf(bio, "No ALPN negotiated\n");
	}

#ifndef OPENSSL_NO_SRTP
	{
		SRTP_PROTECTION_PROFILE *srtp_profile;

		srtp_profile = SSL_get_selected_srtp_profile(s);
		if (srtp_profile)
			BIO_printf(bio,
			    "SRTP Extension negotiated, profile=%s\n",
			    srtp_profile->name);
	}
#endif

	SSL_SESSION_print(bio, SSL_get_session(s));
	if (cfg.keymatexportlabel != NULL) {
		BIO_printf(bio, "Keying material exporter:\n");
		BIO_printf(bio, "    Label: '%s'\n",
		    cfg.keymatexportlabel);
		BIO_printf(bio, "    Length: %i bytes\n",
		    cfg.keymatexportlen);
		exportedkeymat = malloc(cfg.keymatexportlen);
		if (exportedkeymat != NULL) {
			if (!SSL_export_keying_material(s, exportedkeymat,
				cfg.keymatexportlen,
				cfg.keymatexportlabel,
				strlen(cfg.keymatexportlabel),
				NULL, 0, 0)) {
				BIO_printf(bio, "    Error\n");
			} else {
				BIO_printf(bio, "    Keying material: ");
				for (i = 0; i < cfg.keymatexportlen; i++)
					BIO_printf(bio, "%02X",
					    exportedkeymat[i]);
				BIO_printf(bio, "\n");
			}
			free(exportedkeymat);
		}
	}
	BIO_printf(bio, "---\n");
	X509_free(peer);
	/* flush, or debugging output gets mixed with http response */
	(void) BIO_flush(bio);
}

static int
ocsp_resp_cb(SSL *s, void *arg)
{
	const unsigned char *p;
	int len;
	OCSP_RESPONSE *rsp;
	len = SSL_get_tlsext_status_ocsp_resp(s, &p);
	BIO_puts(arg, "OCSP response: ");
	if (!p) {
		BIO_puts(arg, "no response sent\n");
		return 1;
	}
	rsp = d2i_OCSP_RESPONSE(NULL, &p, len);
	if (!rsp) {
		BIO_puts(arg, "response parse error\n");
		BIO_dump_indent(arg, (char *) p, len, 4);
		return 0;
	}
	BIO_puts(arg, "\n======================================\n");
	OCSP_RESPONSE_print(arg, rsp, 0);
	BIO_puts(arg, "======================================\n");
	OCSP_RESPONSE_free(rsp);
	return 1;
}

static int
ssl_servername_cb(SSL *s, int *ad, void *arg)
{
	tlsextctx *p = (tlsextctx *) arg;
	const char *hn = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);
	if (SSL_get_servername_type(s) != -1)
		p->ack = !SSL_session_reused(s) && hn != NULL;
	else
		BIO_printf(bio_err, "Can't use SSL_get_servername\n");

	return SSL_TLSEXT_ERR_OK;
}

