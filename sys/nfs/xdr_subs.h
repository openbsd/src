/*	$NetBSD: xdr_subs.h,v 1.9 1995/12/19 23:08:30 cgd Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)xdr_subs.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Macros used for conversion to/from xdr representation by nfs...
 * These use the MACHINE DEPENDENT routines ntohl, htonl
 * As defined by "XDR: External Data Representation Standard" RFC1014
 *
 * To simplify the implementation, we use ntohl/htonl even on big-endian
 * machines, and count on them being `#define'd away.  Some of these
 * might be slightly more efficient as quad_t copies on a big-endian,
 * but we cannot count on their alignment anyway.
 */

#define	fxdr_unsigned(t, v)	((t)ntohl((int32_t)(v)))
#define	txdr_unsigned(v)	(htonl((int32_t)(v)))

#define	fxdr_nfstime(f, t) { \
	(t)->ts_sec = ntohl((f)->nfs_sec); \
	if ((f)->nfs_usec != 0xffffffff) \
		(t)->ts_nsec = 1000 * ntohl((f)->nfs_usec); \
	else \
		(t)->ts_nsec = 0; \
}
#define	txdr_nfstime(f, t) { \
	(t)->nfs_sec = htonl((f)->ts_sec); \
	(t)->nfs_usec = htonl((f)->ts_nsec) / 1000; \
}

#define	fxdr_nqtime(f, t) { \
	(t)->ts_sec = ntohl((f)->nq_sec); \
	(t)->ts_nsec = ntohl((f)->nq_nsec); \
}
#define	txdr_nqtime(f, t) { \
	(t)->nq_sec = htonl((f)->ts_sec); \
	(t)->nq_nsec = htonl((f)->ts_nsec); \
}

#define	fxdr_hyper(f, t) { \
	((int32_t *)(t))[_QUAD_HIGHWORD] = ntohl(((int32_t *)(f))[0]); \
	((int32_t *)(t))[_QUAD_LOWWORD] = ntohl(((int32_t *)(f))[1]); \
}
#define	txdr_hyper(f, t) { \
	((int32_t *)(t))[0] = htonl(((int32_t *)(f))[_QUAD_HIGHWORD]); \
	((int32_t *)(t))[1] = htonl(((int32_t *)(f))[_QUAD_LOWWORD]); \
}
