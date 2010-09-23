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

static uint32_t ipsec_util_seq = 0;
static int ipsec_util_pid = -1;

struct sadb_del_args {
	int				is_valid;
	uint32_t			spi[128];
	int				spiidx;
	struct sadb_address		src;
	union {
		struct sockaddr_in	sin4;
		struct sockaddr_in6	sin6;
	} src_sa;
	u_char				src_pad[8]; /* for PFKEY_ALIGN8 */
	struct sadb_address		dst;
	union {
		struct sockaddr_in	sin4;
		struct sockaddr_in6	sin6;
	} dst_sa;
	u_char				dst_pad[8]; /* for PFKEY_ALIGN8 */
};

static void        ipsec_util_prepare (void);
static int         delete_prepare (int, struct sockaddr *, struct sockaddr *, int, struct sadb_del_args *, struct sadb_del_args *);
static int         send_sadb_delete (int, struct sadb_del_args *);
static inline int  address_compar (struct sadb_address *, struct sockaddr *, int);
static int         sadb_del_args_init (struct sadb_del_args *, uint32_t, struct sadb_address *, struct sadb_address *, int);
static int         sockaddr_is_valid (struct sockaddr *);

#ifndef countof
#define	countof(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

struct timeval const KEYSOCK_RCVTIMEO = { .tv_sec = 0, .tv_usec = 500000L };
