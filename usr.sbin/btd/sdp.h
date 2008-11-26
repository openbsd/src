/*	$NetBSD: sdp.h,v 1.4 2008/08/06 14:21:33 plunky Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * sdp.h
 *
 * Copyright (c) 2001-2003, 2008 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: sdp.h,v 1.1 2008/11/26 21:48:30 uwe Exp $
 * $FreeBSD: src/lib/libsdp/sdp.h,v 1.5 2005/05/27 19:11:33 emax Exp $
 */

#ifndef _SDP_H_
#define _SDP_H_

#include <sys/queue.h>
#include <event.h>
#include <string.h>

__BEGIN_DECLS

/*
 * Data representation (page 349)
 */

/* Nil, the null type */
#define SDP_DATA_NIL					0x00

/* Unsigned integer */
#define SDP_DATA_UINT8					0x08
#define SDP_DATA_UINT16					0x09
#define SDP_DATA_UINT32					0x0A
#define SDP_DATA_UINT64					0x0B
#define SDP_DATA_UINT128				0x0C

/* Signed two's-complement integer */
#define SDP_DATA_INT8					0x10
#define SDP_DATA_INT16					0x11
#define SDP_DATA_INT32					0x12
#define SDP_DATA_INT64					0x13
#define SDP_DATA_INT128					0x14

/* UUID, a universally unique identifier */
#define SDP_DATA_UUID16					0x19
#define SDP_DATA_UUID32					0x1A
#define SDP_DATA_UUID128				0x1C

/* Text string */
#define SDP_DATA_STR8					0x25
#define SDP_DATA_STR16					0x26
#define SDP_DATA_STR32					0x27

/* Boolean */
#define SDP_DATA_BOOL					0x28

/*
 * Data element sequence.
 * A data element whose data field is a sequence of data elements
 */
#define SDP_DATA_SEQ8					0x35
#define SDP_DATA_SEQ16					0x36
#define SDP_DATA_SEQ32					0x37

/*
 * Data element alternative.
 * A data element whose data field is a sequence of data elements from
 * which one data element is to be selected.
 */
#define SDP_DATA_ALT8					0x3D
#define SDP_DATA_ALT16					0x3E
#define SDP_DATA_ALT32					0x3F

/* URL, a uniform resource locator */
#define SDP_DATA_URL8					0x45
#define SDP_DATA_URL16					0x46
#define SDP_DATA_URL32					0x47

/*
 * Protocols UUID (short) http://www.bluetoothsig.org/assigned-numbers/sdp.htm
 * BASE UUID 00000000-0000-1000-8000-00805F9B34FB
 */

#define SDP_UUID_PROTOCOL_SDP				0x0001
#define SDP_UUID_PROTOCOL_UDP				0x0002
#define SDP_UUID_PROTOCOL_RFCOMM			0x0003
#define SDP_UUID_PROTOCOL_TCP				0x0004
#define SDP_UUID_PROTOCOL_TCS_BIN			0x0005
#define SDP_UUID_PROTOCOL_TCS_AT			0x0006
#define SDP_UUID_PROTOCOL_OBEX				0x0008
#define SDP_UUID_PROTOCOL_IP				0x0009
#define SDP_UUID_PROTOCOL_FTP				0x000A
#define SDP_UUID_PROTOCOL_HTTP				0x000C
#define SDP_UUID_PROTOCOL_WSP				0x000E
#define SDP_UUID_PROTOCOL_BNEP				0x000F
#define SDP_UUID_PROTOCOL_UPNP				0x0010
#define SDP_UUID_PROTOCOL_HIDP				0x0011
#define SDP_UUID_PROTOCOL_HARDCOPY_CONTROL_CHANNEL	0x0012
#define SDP_UUID_PROTOCOL_HARDCOPY_DATA_CHANNEL		0x0014
#define SDP_UUID_PROTOCOL_HARDCOPY_NOTIFICATION		0x0016
#define SDP_UUID_PROTOCOL_AVCTP				0x0017
#define SDP_UUID_PROTOCOL_AVDTP				0x0019
#define SDP_UUID_PROTOCOL_CMPT				0x001B
#define SDP_UUID_PROTOCOL_UDI_C_PLANE			0x001D
#define SDP_UUID_PROTOCOL_L2CAP				0x0100

/*
 * Service class IDs http://www.bluetoothsig.org/assigned-numbers/sdp.htm
 */

#define SDP_SERVICE_CLASS_SERVICE_DISCOVERY_SERVER	0x1000
#define SDP_SERVICE_CLASS_BROWSE_GROUP_DESCRIPTOR	0x1001
#define SDP_SERVICE_CLASS_PUBLIC_BROWSE_GROUP		0x1002
#define SDP_SERVICE_CLASS_SERIAL_PORT			0x1101
#define SDP_SERVICE_CLASS_LAN_ACCESS_USING_PPP		0x1102
#define SDP_SERVICE_CLASS_DIALUP_NETWORKING		0x1103
#define SDP_SERVICE_CLASS_IR_MC_SYNC			0x1104
#define SDP_SERVICE_CLASS_OBEX_OBJECT_PUSH		0x1105
#define SDP_SERVICE_CLASS_OBEX_FILE_TRANSFER		0x1106
#define SDP_SERVICE_CLASS_IR_MC_SYNC_COMMAND		0x1107
#define SDP_SERVICE_CLASS_HEADSET			0x1108
#define SDP_SERVICE_CLASS_CORDLESS_TELEPHONY		0x1109
#define SDP_SERVICE_CLASS_AUDIO_SOURCE			0x110A
#define SDP_SERVICE_CLASS_AUDIO_SINK			0x110B
#define SDP_SERVICE_CLASS_AV_REMOTE_CONTROL_TARGET	0x110C
#define SDP_SERVICE_CLASS_ADVANCED_AUDIO_DISTRIBUTION	0x110D
#define SDP_SERVICE_CLASS_AV_REMOTE_CONTROL		0x110E
#define SDP_SERVICE_CLASS_VIDEO_CONFERENCING		0x110F
#define SDP_SERVICE_CLASS_INTERCOM			0x1110
#define SDP_SERVICE_CLASS_FAX				0x1111
#define SDP_SERVICE_CLASS_HEADSET_AUDIO_GATEWAY		0x1112
#define SDP_SERVICE_CLASS_WAP				0x1113
#define SDP_SERVICE_CLASS_WAP_CLIENT			0x1114
#define SDP_SERVICE_CLASS_PANU				0x1115
#define SDP_SERVICE_CLASS_NAP				0x1116
#define SDP_SERVICE_CLASS_GN				0x1117
#define SDP_SERVICE_CLASS_DIRECT_PRINTING		0x1118
#define SDP_SERVICE_CLASS_REFERENCE_PRINTING		0x1119
#define SDP_SERVICE_CLASS_IMAGING			0x111A
#define SDP_SERVICE_CLASS_IMAGING_RESPONDER		0x111B
#define SDP_SERVICE_CLASS_IMAGING_AUTOMATIC_ARCHIVE	0x111C
#define SDP_SERVICE_CLASS_IMAGING_REFERENCED_OBJECTS	0x111D
#define SDP_SERVICE_CLASS_HANDSFREE			0x111E
#define SDP_SERVICE_CLASS_HANDSFREE_AUDIO_GATEWAY	0x111F
#define SDP_SERVICE_CLASS_DIRECT_PRINTING_REFERENCE_OBJECTS	0x1120
#define SDP_SERVICE_CLASS_REFLECTED_UI			0x1121
#define SDP_SERVICE_CLASS_BASIC_PRINTING		0x1122
#define SDP_SERVICE_CLASS_PRINTING_STATUS		0x1123
#define SDP_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE	0x1124
#define SDP_SERVICE_CLASS_HARDCOPY_CABLE_REPLACEMENT	0x1125
#define SDP_SERVICE_CLASS_HCR_PRINT			0x1126
#define SDP_SERVICE_CLASS_HCR_SCAN			0x1127
#define SDP_SERVICE_CLASS_COMMON_ISDN_ACCESS		0x1128
#define SDP_SERVICE_CLASS_VIDEO_CONFERENCING_GW		0x1129
#define SDP_SERVICE_CLASS_UDI_MT			0x112A
#define SDP_SERVICE_CLASS_UDI_TA			0x112B
#define SDP_SERVICE_CLASS_AUDIO_VIDEO			0x112C
#define SDP_SERVICE_CLASS_SIM_ACCESS			0x112D
#define SDP_SERVICE_CLASS_PNP_INFORMATION		0x1200
#define SDP_SERVICE_CLASS_GENERIC_NETWORKING		0x1201
#define SDP_SERVICE_CLASS_GENERIC_FILE_TRANSFER		0x1202
#define SDP_SERVICE_CLASS_GENERIC_AUDIO			0x1203
#define SDP_SERVICE_CLASS_GENERIC_TELEPHONY		0x1204
#define SDP_SERVICE_CLASS_UPNP				0x1205
#define SDP_SERVICE_CLASS_UPNP_IP			0x1206
#define SDP_SERVICE_CLASS_ESDP_UPNP_IP_PAN		0x1300
#define SDP_SERVICE_CLASS_ESDP_UPNP_IP_LAP		0x1301
#define SDP_SERVICE_CLASS_ESDP_UPNP_L2CAP		0x1302

/*
 * Universal attribute definitions (page 366) and
 * http://www.bluetoothsig.org/assigned-numbers/sdp.htm
 */

#define SDP_ATTR_RANGE(lo, hi) \
	(uint32_t)(((uint16_t)(lo) << 16) | ((uint16_t)(hi)))

#define SDP_ATTR_SERVICE_RECORD_HANDLE			0x0000
#define SDP_ATTR_SERVICE_CLASS_ID_LIST			0x0001
#define SDP_ATTR_SERVICE_RECORD_STATE			0x0002
#define SDP_ATTR_SERVICE_ID				0x0003
#define SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST		0x0004
#define SDP_ATTR_BROWSE_GROUP_LIST			0x0005
#define SDP_ATTR_LANGUAGE_BASE_ATTRIBUTE_ID_LIST	0x0006
#define SDP_ATTR_SERVICE_INFO_TIME_TO_LIVE		0x0007
#define SDP_ATTR_SERVICE_AVAILABILITY			0x0008
#define SDP_ATTR_BLUETOOTH_PROFILE_DESCRIPTOR_LIST	0x0009
#define SDP_ATTR_DOCUMENTATION_URL			0x000A
#define SDP_ATTR_CLIENT_EXECUTABLE_URL			0x000B
#define SDP_ATTR_ICON_URL				0x000C
#define SDP_ATTR_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS	0x000D
#define SDP_ATTR_GROUP_ID				0x0200
#define SDP_ATTR_IP_SUBNET				0x0200
#define SDP_ATTR_VERSION_NUMBER_LIST			0x0200
#define SDP_ATTR_SERVICE_DATABASE_STATE			0x0201
#define SDP_ATTR_SERVICE_VERSION			0x0300
#define SDP_ATTR_EXTERNAL_NETWORK			0x0301
#define SDP_ATTR_NETWORK				0x0301
#define SDP_ATTR_SUPPORTED_DATA_STORES_LIST		0x0301
#define SDP_ATTR_FAX_CLASS1_SUPPORT			0x0302
#define SDP_ATTR_REMOTE_AUDIO_VOLUME_CONTROL		0x0302
#define SDP_ATTR_FAX_CLASS20_SUPPORT			0x0303
#define SDP_ATTR_SUPPORTED_FORMATS_LIST			0x0303
#define SDP_ATTR_FAX_CLASS2_SUPPORT			0x0304
#define SDP_ATTR_AUDIO_FEEDBACK_SUPPORT			0x0305
#define SDP_ATTR_NETWORK_ADDRESS			0x0306
#define SDP_ATTR_WAP_GATEWAY				0x0307
#define SDP_ATTR_HOME_PAGE_URL				0x0308
#define SDP_ATTR_WAP_STACK_TYPE				0x0309
#define SDP_ATTR_SECURITY_DESCRIPTION			0x030A
#define SDP_ATTR_NET_ACCESS_TYPE			0x030B
#define SDP_ATTR_MAX_NET_ACCESS_RATE			0x030C
#define SDP_ATTR_IPV4_SUBNET				0x030D
#define SDP_ATTR_IPV6_SUBNET				0x030E
#define SDP_ATTR_SUPPORTED_CAPABALITIES			0x0310
#define SDP_ATTR_SUPPORTED_FEATURES			0x0311
#define SDP_ATTR_SUPPORTED_FUNCTIONS			0x0312
#define SDP_ATTR_TOTAL_IMAGING_DATA_CAPACITY		0x0313

/*
 * The offset must be added to the attribute ID base (contained in the
 * LANGUAGE_BASE_ATTRIBUTE_ID_LIST attribute) in order to compute the
 * attribute ID for these attributes.
 */

#define SDP_ATTR_PRIMARY_LANGUAGE_BASE_ID		0x0100
#define SDP_ATTR_SERVICE_NAME_OFFSET			0x0000
#define SDP_ATTR_SERVICE_DESCRIPTION_OFFSET		0x0001
#define SDP_ATTR_PROVIDER_NAME_OFFSET			0x0002

/*
 * Protocol data unit (PDU) format (page 352)
 */

#define SDP_PDU_ERROR_RESPONSE				0x01
#define SDP_PDU_SERVICE_SEARCH_REQUEST			0x02
#define SDP_PDU_SERVICE_SEARCH_RESPONSE			0x03
#define SDP_PDU_SERVICE_ATTRIBUTE_REQUEST		0x04
#define SDP_PDU_SERVICE_ATTRIBUTE_RESPONSE		0x05
#define SDP_PDU_SERVICE_SEARCH_ATTRIBUTE_REQUEST	0x06
#define SDP_PDU_SERVICE_SEARCH_ATTRIBUTE_RESPONSE	0x07

struct sdp_pdu {
	uint8_t		pid;	/* PDU ID - SDP_PDU_xxx */
	uint16_t	tid;	/* transaction ID */
	uint16_t	len;	/* parameters length (in bytes) */
} __attribute__ ((packed));
typedef struct sdp_pdu		sdp_pdu_t;
typedef struct sdp_pdu *	sdp_pdu_p;

/*
 * Error codes for SDP_PDU_ERROR_RESPONSE
 */

#define SDP_ERROR_CODE_INVALID_SDP_VERSION		0x0001
#define SDP_ERROR_CODE_INVALID_SERVICE_RECORD_HANDLE	0x0002
#define SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX		0x0003
#define SDP_ERROR_CODE_INVALID_PDU_SIZE			0x0004
#define SDP_ERROR_CODE_INVALID_CONTINUATION_STATE	0x0005
#define SDP_ERROR_CODE_INSUFFICIENT_RESOURCES		0x0006

/*
 * SDP int128/uint128 parameter
 */

struct int128 {
	int8_t	b[16];
};
typedef struct int128	int128_t;
typedef struct int128	uint128_t;

/*
 * SDP attribute
 */

struct sdp_attr {
	uint16_t	 flags;
#define SDP_ATTR_OK		(0 << 0)
#define SDP_ATTR_INVALID	(1 << 0)
#define SDP_ATTR_TRUNCATED	(1 << 1)
	uint16_t	 attr;  /* SDP_ATTR_xxx */
	uint32_t	 vlen;	/* length of the value[] in bytes */
	uint8_t		*value;	/* base pointer */
};
typedef struct sdp_attr		sdp_attr_t;
typedef struct sdp_attr *	sdp_attr_p;

/******************************************************************************
 * User interface
 *****************************************************************************/

/* Inline versions of get/put byte/short/long. Pointer is advanced */
#define SDP_GET8(b, cp)		do {			\
	(b) = *(const uint8_t *)(cp);			\
	(cp) += sizeof(uint8_t);			\
} while (/* CONSTCOND */0)

#define SDP_GET16(s, cp)	do {			\
	(s) = be16dec(cp);				\
	(cp) += sizeof(uint16_t);			\
} while (/* CONSTCOND */0)

#define SDP_GET32(l, cp)	do {			\
	(l) = be32dec(cp);				\
	(cp) += sizeof(uint32_t);			\
} while (/* CONSTCOND */0)

#define SDP_GET64(l, cp)	do {			\
	(l) = be64dec(cp);				\
	(cp) += sizeof(uint64_t);			\
} while (/* CONSTCOND */0)

#if BYTE_ORDER == LITTLE_ENDIAN
#define SDP_GET128(l, cp)	do {			\
	register const uint8_t *t_cp = (const uint8_t *)(cp);	\
	(l)->b[15] = *t_cp++;				\
	(l)->b[14] = *t_cp++;				\
	(l)->b[13] = *t_cp++;				\
	(l)->b[12] = *t_cp++;				\
	(l)->b[11] = *t_cp++;				\
	(l)->b[10] = *t_cp++;				\
	(l)->b[9]  = *t_cp++;				\
	(l)->b[8]  = *t_cp++;				\
	(l)->b[7]  = *t_cp++;				\
	(l)->b[6]  = *t_cp++;				\
	(l)->b[5]  = *t_cp++;				\
	(l)->b[4]  = *t_cp++;				\
	(l)->b[3]  = *t_cp++;				\
	(l)->b[2]  = *t_cp++;				\
	(l)->b[1]  = *t_cp++;				\
	(l)->b[0]  = *t_cp++;				\
	(cp) += 16;					\
} while (/* CONSTCOND */0)

#define SDP_GET_UUID128(l, cp)	do {			\
	memcpy(&((l)->b), (cp), 16);			\
	(cp) += 16;					\
} while (/* CONSTCOND */0)
#elif BYTE_ORDER == BIG_ENDIAN
#define SDP_GET128(l, cp)	do {			\
	memcpy(&((l)->b), (cp), 16);			\
	(cp) += 16;					\
} while (/* CONSTCOND */0)

#define	SDP_GET_UUID128(l, cp)	SDP_GET128(l, cp)
#else
#error	"Unsupported BYTE_ORDER"
#endif /* BYTE_ORDER */

#define SDP_PUT8(b, cp)		do {			\
	*(uint8_t *)(cp) = (b);				\
	(cp) += sizeof(uint8_t);			\
} while (/* CONSTCOND */0)

#define SDP_PUT16(s, cp)	do {			\
	be16enc((cp), (s));				\
	(cp) += sizeof(uint16_t);			\
} while (/* CONSTCOND */0)

#define SDP_PUT32(s, cp)	do {			\
	be32enc((cp), (s));				\
	(cp) += sizeof(uint32_t);			\
} while (/* CONSTCOND */0)

#define SDP_PUT64(s, cp)	do {			\
	be64enc((cp), (s));				\
	(cp) += sizeof(uint64_t);			\
} while (/* CONSTCOND */0)

#if BYTE_ORDER == LITTLE_ENDIAN
#define SDP_PUT128(l, cp)	do {			\
	register const uint8_t *t_cp = (const uint8_t *)(cp);	\
	*t_cp++ = (l)->b[15];				\
	*t_cp++ = (l)->b[14];				\
	*t_cp++ = (l)->b[13];				\
	*t_cp++ = (l)->b[12];				\
	*t_cp++ = (l)->b[11];				\
	*t_cp++ = (l)->b[10];				\
	*t_cp++ = (l)->b[9];				\
	*t_cp++ = (l)->b[8];				\
	*t_cp++ = (l)->b[7];				\
	*t_cp++ = (l)->b[6];				\
	*t_cp++ = (l)->b[5];				\
	*t_cp++ = (l)->b[4];				\
	*t_cp++ = (l)->b[3];				\
	*t_cp++ = (l)->b[2];				\
	*t_cp++ = (l)->b[1];				\
	*t_cp   = (l)->b[0];				\
	(cp) += 16;					\
} while (/* CONSTCOND */0)

#define SDP_PUT_UUID128(l, cp)	do {			\
	memcpy((cp), &((l)->b), 16);			\
	(cp) += 16;					\
} while (/* CONSTCOND */0)
#elif BYTE_ORDER == BIG_ENDIAN
#define SDP_PUT128(l, cp)	do {			\
	memcpy((cp), &((l)->b), 16);			\
	(cp) += 16;					\
} while (/* CONSTCOND */0)

#define SDP_PUT_UUID128(l, cp)	SDP_PUT128(l, cp)
#else
#error	"Unsupported BYTE_ORDER"
#endif /* BYTE_ORDER */

void *             sdp_open       (bdaddr_t const *l, bdaddr_t const *r);
void *             sdp_open_local (char const *control);
int32_t            sdp_close      (void *xs);
int32_t            sdp_error      (void *xs);

int32_t            sdp_search     (void *xs,
                                   uint32_t plen, uint16_t const *pp,
                                   uint32_t alen, uint32_t const *ap,
                                   uint32_t vlen, sdp_attr_t *vp);

char const *       sdp_attr2desc  (uint16_t attr);
char const *       sdp_uuid2desc  (uint16_t uuid);
void               sdp_print      (uint32_t level, uint8_t *start,
                                   uint8_t const *end);

struct sdp_session;
struct btdev_attach_args;

int sdp_query(struct sdp_session *, struct btdev_attach_args *, bdaddr_t *,
    bdaddr_t *, const char *);

/******************************************************************************
 * internal interface and utility functions
 *****************************************************************************/

enum sdp_session_state {
	SDP_SESSION_CLOSED,
	SDP_SESSION_OPEN,
	SDP_SESSION_FINISHED
};

struct sdp_session {
	TAILQ_ENTRY(sdp_session) entry;
	struct sdp_state *sdp;
	enum sdp_session_state state;
	bdaddr_t laddr;
	bdaddr_t raddr;
	uint16_t omtu;
	uint16_t imtu;
	uint8_t *req;
	uint8_t *rsp;
	int fd;
	struct event ev;
	/* for legacy SDP code */
	uint8_t *req_e;
	uint8_t *rsp_e;
	uint16_t tid;
	uint8_t cs[16];
	uint32_t cslen;
	int error;
};

static __inline void
be16enc(void *buf, uint16_t u)
{
	uint8_t *p = (uint8_t *)buf;

	p[0] = ((unsigned)u >> 8) & 0xff;
	p[1] = u & 0xff;
}

static __inline uint16_t
be16dec(const void *buf)
{
	const uint8_t *p = (const uint8_t *)buf;

	return ((p[0] << 8) | p[1]);
}

static __inline void
be32enc(void *buf, uint32_t u)
{
	uint8_t *p = (uint8_t *)buf;

	p[0] = (u >> 24) & 0xff;
	p[1] = (u >> 16) & 0xff;
	p[2] = (u >> 8) & 0xff;
	p[3] = u & 0xff;
}

static __inline uint32_t
be32dec(const void *buf)
{
	const uint8_t *p = (const uint8_t *)buf;

	return ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

static __inline void
be64enc(void *buf, uint64_t u)
{
	uint8_t *p = (uint8_t *)buf;

	be32enc(p, (uint32_t)(u >> 32));
	be32enc(p + 4, (uint32_t)(u & 0xffffffffULL));
}

static __inline uint64_t
be64dec(const void *buf)
{
	const uint8_t *p = (const uint8_t *)buf;

	return (((uint64_t)be32dec(p) << 32) | be32dec(p + 4));
}

__END_DECLS

#endif /* ndef _SDP_H_ */
