/*
 * Copyright (c) 2019 Martijn van Duren <martijn@openbsd.org>
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

#include <stdint.h>

#define AGENTX_PDU_FLAG_INSTANCE_REGISTRATION (1 << 0)
#define AGENTX_PDU_FLAG_NEW_INDEX (1 << 1)
#define AGENTX_PDU_FLAG_ANY_INDEX (1 << 2)
#define AGENTX_PDU_FLAG_NON_DEFAULT_CONTEXT (1 << 3)
#define AGENTX_PDU_FLAG_NETWORK_BYTE_ORDER (1 << 4)

#define AGENTX_PRIORITY_DEFAULT 127

enum agentx_byte_order {
	AGENTX_BYTE_ORDER_BE,
	AGENTX_BYTE_ORDER_LE
};

#if BYTE_ORDER == BIG_ENDIAN
#define AGENTX_BYTE_ORDER_NATIVE AGENTX_BYTE_ORDER_BE
#else
#define AGENTX_BYTE_ORDER_NATIVE AGENTX_BYTE_ORDER_LE
#endif

enum agentx_pdu_type {
	AGENTX_PDU_TYPE_OPEN		= 1,
	AGENTX_PDU_TYPE_CLOSE		= 2,
	AGENTX_PDU_TYPE_REGISTER	= 3,
	AGENTX_PDU_TYPE_UNREGISTER	= 4,
	AGENTX_PDU_TYPE_GET		= 5,
	AGENTX_PDU_TYPE_GETNEXT		= 6,
	AGENTX_PDU_TYPE_GETBULK		= 7,
	AGENTX_PDU_TYPE_TESTSET		= 8,
	AGENTX_PDU_TYPE_COMMITSET	= 9,
	AGENTX_PDU_TYPE_UNDOSET		= 10,
	AGENTX_PDU_TYPE_CLEANUPSET	= 11,
	AGENTX_PDU_TYPE_NOTIFY		= 12,
	AGENTX_PDU_TYPE_PING		= 13,
	AGENTX_PDU_TYPE_INDEXALLOCATE	= 14,
	AGENTX_PDU_TYPE_INDEXDEALLOCATE	= 15,
	AGENTX_PDU_TYPE_ADDAGENTCAPS	= 16,
	AGENTX_PDU_TYPE_REMOVEAGENTCAPS	= 17,
	AGENTX_PDU_TYPE_RESPONSE	= 18
};

enum agentx_pdu_error {
	AGENTX_PDU_ERROR_NOERROR		= 0,
	AGENTX_PDU_ERROR_GENERR			= 5,
	AGENTX_PDU_ERROR_NOACCESS		= 6,
	AGENTX_PDU_ERROR_WRONGTYPE		= 7,
	AGENTX_PDU_ERROR_WRONGLENGTH		= 8,
	AGENTX_PDU_ERROR_WRONGENCODING		= 9,
	AGENTX_PDU_ERROR_WRONGVALUE		= 10,
	AGENTX_PDU_ERROR_NOCREATION		= 11,
	AGENTX_PDU_ERROR_INCONSISTENTVALUE	= 12,
	AGENTX_PDU_ERROR_RESOURCEUNAVAILABLE	= 13,
	AGENTX_PDU_ERROR_COMMITFAILED		= 14,
	AGENTX_PDU_ERROR_UNDOFAILED		= 15,
	AGENTX_PDU_ERROR_NOTWRITABLE		= 17,
	AGENTX_PDU_ERROR_INCONSISTENTNAME	= 18,
	AGENTX_PDU_ERROR_OPENFAILED		= 256,
	AGENTX_PDU_ERROR_NOTOPEN		= 257,
	AGENTX_PDU_ERROR_INDEXWRONGTYPE		= 258,
	AGENTX_PDU_ERROR_INDEXALREADYALLOCATED	= 259,
	AGENTX_PDU_ERROR_INDEXNONEAVAILABLE	= 260,
	AGENTX_PDU_ERROR_INDEXNOTALLOCATED	= 261,
	AGENTX_PDU_ERROR_UNSUPPORTEDCONETXT	= 262,
	AGENTX_PDU_ERROR_DUPLICATEREGISTRATION	= 263,
	AGENTX_PDU_ERROR_UNKNOWNREGISTRATION	= 264,
	AGENTX_PDU_ERROR_UNKNOWNAGENTCAPS	= 265,
	AGENTX_PDU_ERROR_PARSEERROR		= 266,
	AGENTX_PDU_ERROR_REQUESTDENIED		= 267,
	AGENTX_PDU_ERROR_PROCESSINGERROR	= 268
};

enum agentx_data_type {
	AGENTX_DATA_TYPE_INTEGER	= 2,
	AGENTX_DATA_TYPE_OCTETSTRING	= 4,
	AGENTX_DATA_TYPE_NULL		= 5,
	AGENTX_DATA_TYPE_OID		= 6,
	AGENTX_DATA_TYPE_IPADDRESS	= 64,
	AGENTX_DATA_TYPE_COUNTER32	= 65,
	AGENTX_DATA_TYPE_GAUGE32	= 66,
	AGENTX_DATA_TYPE_TIMETICKS	= 67,
	AGENTX_DATA_TYPE_OPAQUE		= 68,
	AGENTX_DATA_TYPE_COUNTER64	= 70,
	AGENTX_DATA_TYPE_NOSUCHOBJECT	= 128,
	AGENTX_DATA_TYPE_NOSUCHINSTANCE	= 129,
	AGENTX_DATA_TYPE_ENDOFMIBVIEW	= 130
};

enum agentx_close_reason {
	AGENTX_CLOSE_OTHER		= 1,
	AGENTX_CLOSEN_PARSEERROR	= 2,
	AGENTX_CLOSE_PROTOCOLERROR	= 3,
	AGENTX_CLOSE_TIMEOUTS		= 4,
	AGENTX_CLOSE_SHUTDOWN		= 5,
	AGENTX_CLOSE_BYMANAGER		= 6
};

struct agentx {
	int ax_fd;
	enum agentx_byte_order ax_byteorder;
	uint8_t *ax_rbuf;
	size_t ax_rblen;
	size_t ax_rbsize;
	uint8_t *ax_wbuf;
	size_t ax_wblen;
	size_t ax_wbtlen;
	size_t ax_wbsize;
	uint32_t *ax_packetids;
	size_t ax_packetidsize;
};

#ifndef AGENTX_PRIMITIVE
#define AGENTX_PRIMITIVE

#define AGENTX_OID_MAX_LEN 128

struct agentx_oid {
	uint8_t aoi_include;
	uint32_t aoi_id[AGENTX_OID_MAX_LEN];
	size_t aoi_idlen;
};

struct agentx_ostring {
	unsigned char *aos_string;
	uint32_t aos_slen;
};
#endif

struct agentx_searchrange {
	struct agentx_oid asr_start;
	struct agentx_oid asr_stop;
};

struct agentx_pdu_header {
	uint8_t	aph_version;
	uint8_t	aph_type;
	uint8_t	aph_flags;
	uint8_t	aph_reserved;
	uint32_t aph_sessionid;
	uint32_t aph_transactionid;
	uint32_t aph_packetid;
	uint32_t aph_plength;
};

struct agentx_varbind {
	enum agentx_data_type avb_type;
	struct agentx_oid avb_oid;
	union agentx_data {
		uint32_t avb_uint32;
		uint64_t avb_uint64;
		struct agentx_ostring avb_ostring;
		struct agentx_oid avb_oid;
	} avb_data;
};

struct agentx_pdu {
	struct agentx_pdu_header ap_header;
	struct agentx_ostring ap_context;
	union {
		struct agentx_pdu_searchrangelist {
			size_t ap_nsr;
			struct agentx_searchrange *ap_sr;
		} ap_srl;
		struct agentx_pdu_getbulk {
			uint16_t ap_nonrep;
			uint16_t ap_maxrep;
			struct agentx_pdu_searchrangelist ap_srl;
		} ap_getbulk;
		struct agentx_pdu_varbindlist {
			struct agentx_varbind *ap_varbind;
			size_t ap_nvarbind;
		} ap_vbl;
		struct agentx_pdu_response {
			uint32_t ap_uptime;
			enum agentx_pdu_error ap_error;
			uint16_t ap_index;
			struct agentx_varbind *ap_varbindlist;
			size_t ap_nvarbind;
		} ap_response;
		void *ap_raw;
	} ap_payload;
};

struct agentx *agentx_new(int);
void agentx_free(struct agentx *);
struct agentx_pdu *agentx_recv(struct agentx *);
ssize_t agentx_send(struct agentx *);
uint32_t agentx_open(struct agentx *, uint8_t, struct agentx_oid *,
    struct agentx_ostring *);
uint32_t agentx_close(struct agentx *, uint32_t, enum agentx_close_reason);
uint32_t agentx_indexallocate(struct agentx *, uint8_t, uint32_t,
    struct agentx_ostring *, struct agentx_varbind *, size_t);
uint32_t agentx_indexdeallocate(struct agentx *, uint32_t,
    struct agentx_ostring *, struct agentx_varbind *, size_t);
uint32_t agentx_addagentcaps(struct agentx *, uint32_t, struct agentx_ostring *,
    struct agentx_oid *, struct agentx_ostring *);
uint32_t agentx_removeagentcaps(struct agentx *, uint32_t,
    struct agentx_ostring *, struct agentx_oid *);
uint32_t agentx_register(struct agentx *, uint8_t, uint32_t,
    struct agentx_ostring *, uint8_t, uint8_t, uint8_t, struct agentx_oid *,
    uint32_t);
uint32_t agentx_unregister(struct agentx *, uint32_t, struct agentx_ostring *,
    uint8_t, uint8_t, struct agentx_oid *, uint32_t);
int agentx_response(struct agentx *, uint32_t, uint32_t, uint32_t,
    struct agentx_ostring *, uint32_t, uint16_t, uint16_t,
    struct agentx_varbind *, size_t);
void agentx_pdu_free(struct agentx_pdu *);
void agentx_varbind_free(struct agentx_varbind *);
const char *agentx_error2string(enum agentx_pdu_error);
const char *agentx_pdutype2string(enum agentx_pdu_type);
const char *agentx_oid2string(struct agentx_oid *);
const char *agentx_oidrange2string(struct agentx_oid *, uint8_t, uint32_t);
const char *agentx_varbind2string(struct agentx_varbind *);
const char *agentx_closereason2string(enum agentx_close_reason);
int agentx_oid_cmp(struct agentx_oid *, struct agentx_oid *);
int agentx_oid_add(struct agentx_oid *, uint32_t);
