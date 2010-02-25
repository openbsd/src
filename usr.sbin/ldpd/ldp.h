/*	$OpenBSD: ldp.h,v 1.3 2010/02/25 17:40:46 claudio Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
 * Copyright (c) 2004, 2005, 2008 Esben Norby <norby@openbsd.org>
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

/* LDP protocol definitions */

#ifndef _LDP_H_
#define _LDP_H_

#include <netinet/in.h>

/* misc */
#define LDP_VERSION		1
#define LDP_PORT		646
#define AllRouters		"224.0.0.2"

#define LDP_MAX_LEN		4096

#define INFINITE_TMR		(-1)
#define LINK_DFLT_HOLDTIME	15
#define TARGETED_DFLT_HOLDTIME	45

#define DEFAULT_HOLDTIME	15
#define MIN_HOLDTIME		1
#define MAX_HOLDTIME		0xffff

#define DEFAULT_KEEPALIVE	180
#define MIN_KEEPALIVE		1
#define MAX_KEEPALIVE		0xffff
#define KEEPALIVE_PER_PERIOD	3

#define	DEFAULT_HELLO_INTERVAL	5
#define	MIN_HELLO_INTERVAL	1
#define	MAX_HELLO_INTERVAL	0xffff	/* XXX */

#define	INIT_DELAY_TMR		15
#define DEFAULT_NBR_TMOUT	86400	/* 24 hours */

/* LDP message types */
#define MSG_TYPE_NOTIFICATION	0x0001
#define MSG_TYPE_HELLO		0x0100
#define MSG_TYPE_INIT		0x0200
#define MSG_TYPE_KEEPALIVE	0x0201
#define MSG_TYPE_ADDR		0x0300
#define MSG_TYPE_ADDRWITHDRAW	0x0301
#define MSG_TYPE_LABELMAPPING	0x0400
#define MSG_TYPE_LABELREQUEST	0x0401
#define MSG_TYPE_LABELWITHDRAW	0x0402
#define MSG_TYPE_LABELRELEASE	0x0403
#define MSG_TYPE_LABELABORTREQ	0x0404

/* LDP TLV types */
#define TLV_TYPE_FEC		0x0100
#define TLV_TYPE_ADDRLIST	0x0101
#define TLV_TYPE_HOPCOUNT	0x0103
#define TLV_TYPE_PATHVECTOR	0x0104
#define TLV_TYPE_GENERICLABEL	0x0200
#define TLV_TYPE_ATMLABEL	0x0201
#define TLV_TYPE_FRLABEL	0x0202
#define TLV_TYPE_STATUS		0x0300
#define TLV_TYPE_EXTSTATUS	0x0301
#define TLV_TYPE_RETURNEDPDU	0x0302
#define TLV_TYPE_RETURNEDMSG	0x0303
#define TLV_TYPE_COMMONHELLO	0x0400
#define TLV_TYPE_IPV4TRANSADDR	0x0401
#define TLV_TYPE_CONFIG		0x0402
#define TLV_TYPE_IPV6TRANSADDR	0x0403
#define TLV_TYPE_COMMONSESSION	0x0500
#define TLV_TYPE_ATMSESSIONPAR	0x0501
#define TLV_TYPE_FRSESSION	0x0502
#define TLV_TYPE_LABELREQUEST	0x0600

/* LDP header */
struct ldp_hdr {
	u_int16_t		version;
	u_int16_t		length;
	u_int32_t		lsr_id;
	u_int16_t		lspace_id;
};

#define	LDP_HDR_SIZE		10
#define	INFINITE_HOLDTIME	0xffff

/* TLV record */
struct ldp_msg {
	u_int16_t	type;
	u_int16_t	length;
	u_int32_t	msgid;
	/* Mandatory Parameters */
	/* Optional Parameters */
};

#define LDP_MSG_LEN		8
#define	TLV_HDR_LEN		4

#define	UNKNOWN_FLAGS_MASK	0xc000
#define	UNKNOWN_FLAG		0x8000
#define	FORWARD_FLAG		0xc000

#define TARGETED_HELLO		0x8000
#define REQUEST_TARG_HELLO	0x4000

struct hello_prms_tlv {
	u_int16_t	type;
	u_int16_t	length;
	u_int16_t	holdtime;
	u_int16_t	reserved;
};

#define	S_SUCCESS	0x00000000
#define	S_BAD_LDP_ID	0x80000001
#define	S_BAD_PROTO_VER	0x80000002
#define	S_BAD_PDU_LEN	0x80000003
#define	S_UNKNOWN_MSG	0x00000004
#define	S_BAD_MSG_LEN	0x80000005
#define	S_UNKNOWN_TLV	0x00000006
#define	S_BAD_TLV_LEN	0x80000007
#define	S_BAD_TLV_VAL	0x80000008
#define	S_HOLDTIME_EXP	0x80000009
#define	S_SHUTDOWN	0x8000000A
#define	S_LOOP_DETECTED	0x0000000B
#define	S_UNKNOWN_FEC	0x0000000C
#define	S_NO_ROUTE	0x0000000D
#define	S_NO_LABEL_RES	0x0000000E
#define	S_AVAILABLE	0x0000000F
#define	S_NO_HELLO	0x80000010
#define	S_PARM_ADV_MODE	0x80000011
#define	S_MAX_PDU_LEN	0x80000012
#define	S_PARM_L_RANGE	0x80000013
#define	S_KEEPALIVE_TMR	0x80000014
#define	S_LAB_REQ_ABRT	0x00000015
#define	S_MISS_MSG	0x00000016
#define	S_UNSUP_ADDR	0x00000017

struct sess_prms_tlv {
	u_int16_t	type;
	u_int16_t	length;
	u_int16_t	proto_version;
	u_int16_t	keepalive_time;
	u_int8_t	reserved;
	u_int8_t	pvlim;
	u_int16_t	max_pdu_len;
	u_int32_t	lsr_id;
	u_int16_t	lspace_id;
};

#define SESS_PRMS_SIZE		18

struct status_tlv {
	u_int16_t	type;
	u_int16_t	length;
	u_int32_t	status_code;
	u_int32_t	msg_id;
	u_int16_t	msg_type;
};

#define STATUS_SIZE		14
#define STATUS_TLV_LEN		10
#define	STATUS_FATAL		0x80000000

struct address_list_tlv {
	u_int16_t	type;
	u_int16_t	length;
	u_int16_t	family;
	/* address entries */
};

#define	BASIC_LABEL_MAP_LEN	24

#define	ADDR_IPV4		0x1
#define	ADDR_IPV6		0x2

struct fec_tlv {
	u_int16_t	type;
	u_int16_t	length;
	/* fec elm entries */
};

/* This struct is badly aligned so use two 32 bit fields */
struct fec_elm {
	u_int32_t	hdr;
	u_int32_t	addr;
};

#define FEC_ELM_MIN_LEN		4

#define	FEC_WILDCARD		0x01
#define	FEC_PREFIX		0x02
#define	FEC_ADDRESS		0x03

#define	FEC_IPV4		0x0001

struct label_tlv {
	u_int16_t	type;
	u_int16_t	length;
	u_int32_t	label;
};

#define LABEL_TLV_LEN		8

struct hello_opt_parms_tlv {
	u_int16_t	type;
	u_int16_t	length;
	u_int32_t	value;
};

#define	NO_LABEL		UINT_MAX

#endif /* !_LDP_H_ */
