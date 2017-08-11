/* $OpenBSD: ssl_tlsext.h,v 1.5 2017/08/11 20:14:13 doug Exp $ */
/*
 * Copyright (c) 2016, 2017 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2017 Doug Hogan <doug@openbsd.org>
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

int tlsext_ri_clienthello_needs(SSL *s);
int tlsext_ri_clienthello_build(SSL *s, CBB *cbb);
int tlsext_ri_clienthello_parse(SSL *s, CBS *cbs, int *alert);
int tlsext_ri_serverhello_needs(SSL *s);
int tlsext_ri_serverhello_build(SSL *s, CBB *cbb);
int tlsext_ri_serverhello_parse(SSL *s, CBS *cbs, int *alert);

int tlsext_sni_clienthello_needs(SSL *s);
int tlsext_sni_clienthello_build(SSL *s, CBB *cbb);
int tlsext_sni_clienthello_parse(SSL *s, CBS *cbs, int *alert);
int tlsext_sni_serverhello_needs(SSL *s);
int tlsext_sni_serverhello_build(SSL *s, CBB *cbb);
int tlsext_sni_serverhello_parse(SSL *s, CBS *cbs, int *alert);

int tlsext_ec_clienthello_needs(SSL *s);
int tlsext_ec_clienthello_build(SSL *s, CBB *cbb);
int tlsext_ec_clienthello_parse(SSL *s, CBS *cbs, int *alert);
int tlsext_ec_serverhello_needs(SSL *s);
int tlsext_ec_serverhello_build(SSL *s, CBB *cbb);
int tlsext_ec_serverhello_parse(SSL *s, CBS *cbs, int *alert);

int tlsext_ecpf_clienthello_needs(SSL *s);
int tlsext_ecpf_clienthello_build(SSL *s, CBB *cbb);
int tlsext_ecpf_clienthello_parse(SSL *s, CBS *cbs, int *alert);
int tlsext_ecpf_serverhello_needs(SSL *s);
int tlsext_ecpf_serverhello_build(SSL *s, CBB *cbb);
int tlsext_ecpf_serverhello_parse(SSL *s, CBS *cbs, int *alert);


int tlsext_clienthello_build(SSL *s, CBB *cbb);
int tlsext_clienthello_parse_one(SSL *s, CBS *cbs, uint16_t tlsext_type,
    int *alert);

int tlsext_serverhello_build(SSL *s, CBB *cbb);
int tlsext_serverhello_parse_one(SSL *s, CBS *cbs, uint16_t tlsext_type,
    int *alert);
