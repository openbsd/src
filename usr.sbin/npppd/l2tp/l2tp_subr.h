/*	$OpenBSD: l2tp_subr.h,v 1.2 2010/07/01 03:38:17 yasuoka Exp $	*/
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
#ifndef	L2TP_SUBR_H
#define	L2TP_SUBR_H	1
/* $Id: l2tp_subr.h,v 1.2 2010/07/01 03:38:17 yasuoka Exp $ */

/**
 * structure of L2TP Attribute Value Pair (AVP) packet header
 */
struct l2tp_avp
{
#if	BYTE_ORDER == LITTLE_ENDIAN
	uint16_t	length:10,
			rsvd:4,
			is_hidden:1,
			is_mandatory:1;
#else
	uint16_t	is_mandatory:1,
			is_hidden:1,
			rsvd:4,
			length:10;
#endif
	uint16_t	vendor_id;
	uint16_t	attr_type;
	u_char 		attr_value[0];
} __attribute__((__packed__)) ;

#define	avp_attr_length(avp)	((avp)->length - 6)

static inline uint16_t
avp_get_val16(struct l2tp_avp *avp)
{
	return (avp->attr_value[0] << 8) | avp->attr_value[1];
}
static inline uint32_t
avp_get_val32(struct l2tp_avp *avp)
{
	return (avp->attr_value[0] << 24) | (avp->attr_value[1] << 16) |
		(avp->attr_value[2] << 8) | avp->attr_value[3];
}

static inline void
avp_set_val16(struct l2tp_avp *avp, uint16_t val)
{
	avp->attr_value[0] = val >> 8;
	avp->attr_value[1] = val & 0xff;
}

static inline void
avp_set_val32(struct l2tp_avp *avp, uint32_t val)
{
	avp->attr_value[0] = val >> 24;
	avp->attr_value[1] = val >> 16;
	avp->attr_value[2] = val >> 8;
	avp->attr_value[3] = val & 0xff;
}

static inline int
short_cmp(const void *m, const void *n)
{
	return (int)((int)m - (int)n);
}

static inline uint32_t
short_hash(const void *v, int sz)
{
	return (int)v % sz;
}

/* 
 * macro to check AVP size.
 * Prepare 1) char emes[256] for error message, 2) size_check_failed label
 * before use this macro.
 */
#define	AVP_SIZE_CHECK(avp, op, exp)					\
	    if (!((avp)->length op (exp))) {				\
		    snprintf(emes, sizeof(emes),			\
			"invalid packet size %s %d" #op "%d)",		\
			avp_attr_type_string((avp)->attr_type),		\
			(avp)->length, (exp));				\
		    goto size_check_failed;				\
	    }

#ifdef __cplusplus
extern "C" {
#endif

int              avp_enum (struct l2tp_avp *, const u_char *, int, int);
const char       *avp_attr_type_string (int);
struct l2tp_avp  *avp_find_message_type_avp(struct l2tp_avp *, const u_char *, int);
struct l2tp_avp  *avp_find(struct l2tp_avp *, const u_char *, int, uint16_t, uint16_t, int);
int              bytebuf_add_avp (bytebuffer *, struct l2tp_avp *, int);
const char       *avp_mes_type_string (int);
const char *     l2tp_cdn_rcode_string(int);
const char *     l2tp_stopccn_rcode_string(int);
const char *     l2tp_ecode_string(int);

#ifdef __cplusplus
}
#endif

#endif
