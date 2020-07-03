/* $OpenBSD: ssl_tlsext.h,v 1.25 2020/07/03 04:51:59 tb Exp $ */
/*
 * Copyright (c) 2016, 2017 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2017 Doug Hogan <doug@openbsd.org>
 * Copyright (c) 2019 Bob Beck <beck@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef HEADER_SSL_TLSEXT_H
#define HEADER_SSL_TLSEXT_H

/* TLSv1.3 - RFC 8446 Section 4.2. */
#define SSL_TLSEXT_MSG_CH	0x0001	/* ClientHello */
#define SSL_TLSEXT_MSG_SH	0x0002	/* ServerHello */
#define SSL_TLSEXT_MSG_EE	0x0004	/* EncryptedExtension */
#define SSL_TLSEXT_MSG_CT	0x0008	/* Certificate */
#define SSL_TLSEXT_MSG_CR	0x0010	/* CertificateRequest */
#define SSL_TLSEXT_MSG_NST	0x0020	/* NewSessionTicket */
#define SSL_TLSEXT_MSG_HRR	0x0040	/* HelloRetryRequest */

__BEGIN_HIDDEN_DECLS

int tlsext_alpn_client_needs(SSL *s, uint16_t msg_type);
int tlsext_alpn_client_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_alpn_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);
int tlsext_alpn_server_needs(SSL *s, uint16_t msg_type);
int tlsext_alpn_server_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_alpn_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);

int tlsext_ri_client_needs(SSL *s, uint16_t msg_type);
int tlsext_ri_client_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_ri_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);
int tlsext_ri_server_needs(SSL *s, uint16_t msg_type);
int tlsext_ri_server_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_ri_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);

int tlsext_sigalgs_client_needs(SSL *s, uint16_t msg_type);
int tlsext_sigalgs_client_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_sigalgs_client_parse(SSL *s, uint16_t msg_type, CBS *cbs,
    int *alert);
int tlsext_sigalgs_server_needs(SSL *s, uint16_t msg_type);
int tlsext_sigalgs_server_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_sigalgs_server_parse(SSL *s, uint16_t msg_type, CBS *cbs,
    int *alert);

int tlsext_sni_client_needs(SSL *s, uint16_t msg_type);
int tlsext_sni_client_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_sni_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);
int tlsext_sni_server_needs(SSL *s, uint16_t msg_type);
int tlsext_sni_server_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_sni_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);
int tlsext_sni_is_valid_hostname(CBS *cbs);

int tlsext_supportedgroups_client_needs(SSL *s, uint16_t msg_type);
int tlsext_supportedgroups_client_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_supportedgroups_client_parse(SSL *s, uint16_t msg_type, CBS *cbs,
    int *alert);
int tlsext_supportedgroups_server_needs(SSL *s, uint16_t msg_type);
int tlsext_supportedgroups_server_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_supportedgroups_server_parse(SSL *s, uint16_t msg_type, CBS *cbs,
    int *alert);

int tlsext_ecpf_client_needs(SSL *s, uint16_t msg_type);
int tlsext_ecpf_client_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_ecpf_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);
int tlsext_ecpf_server_needs(SSL *s, uint16_t msg_type);
int tlsext_ecpf_server_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_ecpf_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);

int tlsext_ocsp_client_needs(SSL *s, uint16_t msg_type);
int tlsext_ocsp_client_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_ocsp_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);
int tlsext_ocsp_server_needs(SSL *s, uint16_t msg_type);
int tlsext_ocsp_server_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_ocsp_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);

int tlsext_sessionticket_client_needs(SSL *s, uint16_t msg_type);
int tlsext_sessionticket_client_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_sessionticket_client_parse(SSL *s, uint16_t msg_type, CBS *cbs,
     int *alert);
int tlsext_sessionticket_server_needs(SSL *s, uint16_t msg_type);
int tlsext_sessionticket_server_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_sessionticket_server_parse(SSL *s, uint16_t msg_type, CBS *cbs,
    int *alert);

int tlsext_versions_client_needs(SSL *s, uint16_t msg_type);
int tlsext_versions_client_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_versions_client_parse(SSL *s, uint16_t msg_type, CBS *cbs,
    int *alert);
int tlsext_versions_server_needs(SSL *s, uint16_t msg_type);
int tlsext_versions_server_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_versions_server_parse(SSL *s, uint16_t msg_type, CBS *cbs,
    int *alert);

int tlsext_keyshare_client_needs(SSL *s, uint16_t msg_type);
int tlsext_keyshare_client_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_keyshare_client_parse(SSL *s, uint16_t msg_type, CBS *cbs,
    int *alert);
int tlsext_keyshare_server_needs(SSL *s, uint16_t msg_type);
int tlsext_keyshare_server_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_keyshare_server_parse(SSL *s, uint16_t msg_type, CBS *cbs,
    int *alert);

int tlsext_cookie_client_needs(SSL *s, uint16_t msg_type);
int tlsext_cookie_client_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_cookie_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);
int tlsext_cookie_server_needs(SSL *s, uint16_t msg_type);
int tlsext_cookie_server_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_cookie_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);

#ifndef OPENSSL_NO_SRTP
int tlsext_srtp_client_needs(SSL *s, uint16_t msg_type);
int tlsext_srtp_client_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_srtp_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);
int tlsext_srtp_server_needs(SSL *s, uint16_t msg_type);
int tlsext_srtp_server_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_srtp_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);
#endif

int tlsext_client_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);

int tlsext_server_build(SSL *s, uint16_t msg_type, CBB *cbb);
int tlsext_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);

struct tls_extension *tls_extension_find(uint16_t, size_t *);
int tlsext_extension_seen(SSL *s, uint16_t);
__END_HIDDEN_DECLS

#endif
