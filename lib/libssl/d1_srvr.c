/* $OpenBSD: d1_srvr.c,v 1.94 2018/08/30 16:56:16 jsing Exp $ */
/*
 * DTLS implementation written by Nagendra Modadugu
 * (nagendra@cs.stanford.edu) for the OpenSSL project 2005.
 */
/* ====================================================================
 * Copyright (c) 1999-2007 The OpenSSL Project.  All rights reserved.
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
 *    openssl-core@OpenSSL.org.
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

#include <stdio.h>

#include "ssl_locl.h"

#include <openssl/bn.h>
#include <openssl/buffer.h>
#include <openssl/dh.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/objects.h>
#include <openssl/x509.h>

static const SSL_METHOD_INTERNAL DTLSv1_server_method_internal_data = {
	.version = DTLS1_VERSION,
	.min_version = DTLS1_VERSION,
	.max_version = DTLS1_VERSION,
	.ssl_new = dtls1_new,
	.ssl_clear = dtls1_clear,
	.ssl_free = dtls1_free,
	.ssl_accept = ssl3_accept,
	.ssl_connect = ssl_undefined_function,
	.get_ssl_method = dtls1_get_server_method,
	.get_timeout = dtls1_default_timeout,
	.ssl_version = ssl_undefined_void_function,
	.ssl_renegotiate = ssl3_renegotiate,
	.ssl_renegotiate_check = ssl3_renegotiate_check,
	.ssl_get_message = dtls1_get_message,
	.ssl_read_bytes = dtls1_read_bytes,
	.ssl_write_bytes = dtls1_write_app_data_bytes,
	.ssl3_enc = &DTLSv1_enc_data,
};

static const SSL_METHOD DTLSv1_server_method_data = {
	.ssl_dispatch_alert = dtls1_dispatch_alert,
	.num_ciphers = ssl3_num_ciphers,
	.get_cipher = dtls1_get_cipher,
	.get_cipher_by_char = ssl3_get_cipher_by_char,
	.put_cipher_by_char = ssl3_put_cipher_by_char,
	.internal = &DTLSv1_server_method_internal_data,
};

const SSL_METHOD *
DTLSv1_server_method(void)
{
	return &DTLSv1_server_method_data;
}

const SSL_METHOD *
dtls1_get_server_method(int ver)
{
	if (ver == DTLS1_VERSION)
		return (DTLSv1_server_method());
	return (NULL);
}

int
dtls1_send_hello_verify_request(SSL *s)
{
	CBB cbb, verify, cookie;

	memset(&cbb, 0, sizeof(cbb));

	if (S3I(s)->hs.state == DTLS1_ST_SW_HELLO_VERIFY_REQUEST_A) {
		if (s->ctx->internal->app_gen_cookie_cb == NULL ||
		    s->ctx->internal->app_gen_cookie_cb(s, D1I(s)->cookie,
			&(D1I(s)->cookie_len)) == 0) {
			SSLerror(s, ERR_R_INTERNAL_ERROR);
			return 0;
		}

		if (!ssl3_handshake_msg_start(s, &cbb, &verify,
		    DTLS1_MT_HELLO_VERIFY_REQUEST))
			goto err;
		if (!CBB_add_u16(&verify, s->version))
			goto err;
		if (!CBB_add_u8_length_prefixed(&verify, &cookie))
			goto err;
		if (!CBB_add_bytes(&cookie, D1I(s)->cookie, D1I(s)->cookie_len))
			goto err;
		if (!ssl3_handshake_msg_finish(s, &cbb))
			goto err;

		S3I(s)->hs.state = DTLS1_ST_SW_HELLO_VERIFY_REQUEST_B;
	}

	/* S3I(s)->hs.state = DTLS1_ST_SW_HELLO_VERIFY_REQUEST_B */
	return (ssl3_handshake_write(s));

 err:
	CBB_cleanup(&cbb);

	return (-1);
}
