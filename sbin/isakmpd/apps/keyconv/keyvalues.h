/*	$OpenBSD: keyvalues.h,v 1.1 2001/08/22 15:29:22 ho Exp $	*/

/*
 * Copyright (c) 2001 Hakan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Values taken from /usr/local/include/dns/keyvalues.h (BIND 8/9) */

#ifndef DNS_KEYALG_RSAMD5
#  define DNS_KEYALG_RSAMD5	1
#  define DNS_KEYALG_RSA	DNS_KEYALG_RSAMD5
#  define DNS_KEYALG_DH		2
#  define DNS_KEYALG_DSA	3
#  define DNS_KEYALG_DSS	DNS_KEYALG_DSA
#endif

#ifndef DNS_KEYOWNER_ENTITY
#  define DNS_KEYOWNER_USER	0x0000
#  define DNS_KEYOWNER_ZONE	0x0100
#  define DNS_KEYOWNER_ENTITY	0x0200
#  define DNS_KEYOWNER_RESERVED	0x0300
#endif

#ifndef DNS_KEYPROTO_DNSSEC
#  define DNS_KEYPROTO_RESERVED	0
#  define DNS_KEYPROTO_TLS	1
#  define DNS_KEYPROTO_EMAIL	2
#  define DNS_KEYPROTO_DNSSEC	3
#  define DNS_KEYPROTO_IPSEC	4
#  define DNS_KEYPROTO_ANY	255
#endif

