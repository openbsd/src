/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 */
#ifndef	RADIUS_REQ_H
#define	RADIUS_REQ_H 1

/** RADIUS 共有秘密鍵の最大長 */
#define MAX_RADIUS_SECRET				128

/** RADIUS サーバの最大数 */
#define MAX_RADIUS_SERVERS				16

/** RADIUS 要求が失敗した */
#define	RADIUS_REQUST_ERROR				0x0001

/** RADIUS 要求がタイムアウトした */
#define	RADIUS_REQUST_TIMEOUT				0x0002

/** Authenticator チェックは OK  */
#define	RADIUS_REQUST_CHECK_AUTHENTICTOR_OK		0x0010

/** Authenticator チェックはしていない  */
#define	RADIUS_REQUST_CHECK_AUTHENTICTOR_NO_CHECK	0x0020

/** RADIUS 応答を受信するコールバック関数の型 */
typedef void (radius_response)(void *context, RADIUS_PACKET *pkt, int flags);

/** RADIUS 要求/応答を行うためのコンテキストを示す型 */
typedef void * RADIUS_REQUEST_CTX;

/** RADIUS 要求の設定を示す型 */
typedef struct _radius_req_setting 
{
	/** サーバ配列 */
	struct {
		union {
			/** サーバの IPv6 アドレス */
			struct sockaddr_in6 sin6;
			/** サーバの IPv4 アドレス */
			struct sockaddr_in sin4;
		}	/** サーバのアドレス */ peer;
		union {
			/** サーバの IPv6 アドレス */
			struct sockaddr_in6 sin6;
			/** サーバの IPv4 アドレス */
			struct sockaddr_in sin4;
		}	/** サーバのアドレス */ sock;
		char	secret[MAX_RADIUS_SECRET];
		/** このアドレスが有効かどうか */
		int	enabled;
	} server[MAX_RADIUS_SERVERS];
	/**
	 * 現在使用しているサーバのインデックス。
	 * <p>
	 * これを変更する仕組みは、radius_req.c では実装していません。npppd
	 * では npppd.c、npppd_auth.c
	 * あたりで実装しています。</p>
	 */
	int curr_server;
	/** リクエストタイムアウト(秒) */
	int timeout;
} radius_req_setting;

#ifdef __cplusplus
extern "C" {
#endif

int   radius_prepare (radius_req_setting *, void *, RADIUS_REQUEST_CTX *, radius_response *, int);
void  radius_request (RADIUS_REQUEST_CTX, RADIUS_PACKET *);
void  radius_cancel_request (RADIUS_REQUEST_CTX);
const char *radius_get_server_secret(RADIUS_REQUEST_CTX);
struct sockaddr *radius_get_server_address(RADIUS_REQUEST_CTX);
int radius_prepare_nas_address(radius_req_setting *, RADIUS_PACKET *);

#ifdef __cplusplus
}
#endif

#endif
