/*	$OpenBSD: snmp.h,v 1.15 2018/06/17 18:19:59 rob Exp $	*/

/*
 * Copyright (c) 2007, 2008, 2012 Reyk Floeter <reyk@openbsd.org>
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

#ifndef SNMPD_SNMP_H
#define SNMPD_SNMP_H

#include <sys/types.h>
#include <endian.h>

/*
 * SNMP IMSG interface
 */

#define SNMP_MAX_OID_STRLEN	128	/* max size of the OID _string_ */
#define SNMP_SOCKET		"/var/run/snmpd.sock"
#define AGENTX_SOCKET		"/var/run/agentx.sock"
#define SNMP_RESTRICTED_SOCKET	"/var/run/snmpd.rsock"

enum snmp_type {
	SNMP_IPADDR		= 0,
	SNMP_COUNTER32		= 1,
	SNMP_GAUGE32		= 2,
	SNMP_UNSIGNED32		= 2,
	SNMP_TIMETICKS		= 3,
	SNMP_OPAQUE		= 4,
	SNMP_NSAPADDR		= 5,
	SNMP_COUNTER64		= 6,
	SNMP_UINTEGER32		= 7,

	SNMP_INTEGER32		= 100,
	SNMP_BITSTRING		= 101,
	SNMP_OCTETSTRING	= 102,
	SNMP_NULL		= 103,
	SNMP_OBJECT		= 104
};

enum snmp_imsg_ctl {
	IMSG_SNMP_DUMMY		= 1000,	/* something that works everywhere */
	IMSG_SNMP_ELEMENT,
	IMSG_SNMP_END,
	IMSG_SNMP_LOCK,			/* enable restricted mode */
	IMSG_SNMP_AGENTX
};

struct snmp_imsg_hdr {
	u_int32_t	 imsg_type;
	u_int16_t	 imsg_len;
	u_int16_t	 imsg_flags;
	u_int32_t	 imsg_peerid;
	u_int32_t	 imsg_pid;
};

struct snmp_imsg {
	char		 snmp_oid[SNMP_MAX_OID_STRLEN];
	u_int8_t	 snmp_type;
	u_int16_t	 snmp_len;
};

/*
 * SNMP BER types
 */

enum snmp_version {
	SNMP_V1			= 0,
	SNMP_V2			= 1,	/* SNMPv2c */
	SNMP_V3			= 3
};

enum snmp_context {
	SNMP_C_GETREQ		= 0,
	SNMP_C_GETNEXTREQ	= 1,
	SNMP_C_GETRESP		= 2,
	SNMP_C_SETREQ		= 3,
	SNMP_C_TRAP		= 4,

	/* SNMPv2 */
	SNMP_C_GETBULKREQ	= 5,
	SNMP_C_INFORMREQ	= 6,
	SNMP_C_TRAPV2		= 7,
	SNMP_C_REPORT		= 8
};

enum snmp_application {
	SNMP_T_IPADDR		= 0,
	SNMP_T_COUNTER32	= 1,
	SNMP_T_GAUGE32		= 2,
	SNMP_T_UNSIGNED32	= 2,
	SNMP_T_TIMETICKS	= 3,
	SNMP_T_OPAQUE		= 4,
	SNMP_T_NSAPADDR		= 5,
	SNMP_T_COUNTER64	= 6,
	SNMP_T_UINTEGER32	= 7
};

enum snmp_generic_trap {
	SNMP_TRAP_COLDSTART	= 0,
	SNMP_TRAP_WARMSTART	= 1,
	SNMP_TRAP_LINKDOWN	= 2,
	SNMP_TRAP_LINKUP	= 3,
	SNMP_TRAP_AUTHFAILURE	= 4,
	SNMP_TRAP_EGPNEIGHLOSS	= 5,
	SNMP_TRAP_ENTERPRISE	= 6
};

enum snmp_error {
	SNMP_ERROR_NONE		= 0,
	SNMP_ERROR_TOOBIG	= 1,
	SNMP_ERROR_NOSUCHNAME	= 2,
	SNMP_ERROR_BADVALUE	= 3,
	SNMP_ERROR_READONLY	= 4,
	SNMP_ERROR_GENERR	= 5,

	/* SNMPv2 */
	SNMP_ERROR_NOACCESS	= 6,
	SNMP_ERROR_WRONGTYPE	= 7,
	SNMP_ERROR_WRONGLENGTH	= 8,
	SNMP_ERROR_WRONGENC	= 9,
	SNMP_ERROR_WRONGVALUE	= 10,
	SNMP_ERROR_NOCREATION	= 11,
	SNMP_ERROR_INCONVALUE	= 12,
	SNMP_ERROR_RESUNAVAIL	= 13, /* EGAIN */
	SNMP_ERROR_COMMITFAILED	= 14,
	SNMP_ERROR_UNDOFAILED	= 15,
	SNMP_ERROR_AUTHERROR	= 16,
	SNMP_ERROR_NOTWRITABLE	= 17,
	SNMP_ERROR_INCONNAME	= 18
};

enum snmp_security_model {
	SNMP_SEC_ANY = 0,
	SNMP_SEC_SNMPv1 = 1,
	SNMP_SEC_SNMPv2c = 2,
	SNMP_SEC_USM = 3,
	SNMP_SEC_TSM = 4
};

#define SNMP_MSGFLAG_AUTH	0x01
#define SNMP_MSGFLAG_PRIV	0x02
#define SNMP_MSGFLAG_SECMASK	(SNMP_MSGFLAG_AUTH | SNMP_MSGFLAG_PRIV)
#define SNMP_MSGFLAG_REPORT	0x04

#define SNMP_MAX_TIMEWINDOW	150	/* RFC3414 */

#define SNMP_MIN_OID_LEN	2	/* OBJECT */
#define SNMP_MAX_OID_LEN	32	/* OBJECT */

struct snmp_oid {
	u_int32_t	o_id[SNMP_MAX_OID_LEN + 1];
	size_t		o_n;
};

/* AgentX protocol, as outlined in RFC 2741 */

/* version */
#define AGENTX_VERSION			1

/* type */
#define	AGENTX_OPEN			1
#define	AGENTX_CLOSE			2
#define	AGENTX_REGISTER			3
#define	AGENTX_UNREGISTER		4
#define	AGENTX_GET			5
#define	AGENTX_GET_NEXT			6
#define	AGENTX_GET_BULK			7
#define	AGENTX_TEST_SET			8
#define	AGENTX_COMMIT_SET		9
#define	AGENTX_UNDO_SET			10
#define	AGENTX_CLEANUP_SET		11
#define	AGENTX_NOTIFY			12
#define	AGENTX_PING			13
#define	AGENTX_INDEX_ALLOCATE		14
#define	AGENTX_INDEX_DEALLOCATE		15
#define	AGENTX_ADD_AGENT_CAPS		16
#define	AGENTX_REMOVE_AGENT_CAPS	17
#define	AGENTX_RESPONSE			18

/* error return codes */
#define	AGENTX_ERR_NONE				0
#define	AGENTX_ERR_OPEN_FAILED			256
#define	AGENTX_ERR_NOT_OPEN			257
#define	AGENTX_ERR_INDEX_WRONG_TYPE		258
#define	AGENTX_ERR_INDEX_ALREADY_ALLOCATED	259
#define	AGENTX_ERR_INDEX_NONE_AVAILABLE		260
#define	AGENTX_ERR_INDEX_NOT_ALLOCATED		261
#define	AGENTX_ERR_UNSUPPORTED_CONTEXT		262
#define	AGENTX_ERR_DUPLICATE_REGISTRATION	263
#define	AGENTX_ERR_UNKNOWN_REGISTRATION		264
#define	AGENTX_ERR_UNKNOWN_AGENT_CAPS		265
#define	AGENTX_ERR_PARSE_ERROR			266
#define	AGENTX_ERR_REQUEST_DENIED		267
#define	AGENTX_ERR_PROCESSING_ERROR		268

/* flags */
#define	AGENTX_INSTANCE_REGISTRATION	0x01
#define	AGENTX_NEW_INDEX		0x02
#define	AGENTX_ANY_INDEX		0x04
#define	AGENTX_NON_DEFAULT_CONTEXT	0x08
#define	AGENTX_NETWORK_BYTE_ORDER	0x10
#define	AGENTX_FLAGS_MASK		0x1f

/* encoded data types */
#define	AGENTX_INTEGER			2
#define	AGENTX_OCTET_STRING		4
#define	AGENTX_NULL			5
#define	AGENTX_OBJECT_IDENTIFIER	6
#define	AGENTX_IP_ADDRESS		64
#define	AGENTX_COUNTER32		65
#define	AGENTX_GAUGE32			66
#define	AGENTX_TIME_TICKS		67
#define	AGENTX_OPAQUE			68
#define	AGENTX_COUNTER64		70
#define	AGENTX_NO_SUCH_OBJECT		128
#define	AGENTX_NO_SUCH_INSTANCE		129
#define	AGENTX_END_OF_MIB_VIEW		130

/* for registered MIB overlap */
#define	AGENTX_REGISTER_PRIO_DEFAULT	127

/* reasons for request of close */
#define AGENTX_CLOSE_OTHER		1
#define AGENTX_CLOSE_PARSE_ERROR	2
#define AGENTX_CLOSE_PROTOCOL_ERROR	3
#define AGENTX_CLOSE_TIMEOUTS		4
#define AGENTX_CLOSE_SHUTDOWN		5
#define AGENTX_CLOSE_BY_MANAGER		6

#define	AGENTX_DEFAULT_TIMEOUT		3

#define	MIN_OID_LEN		2       /* OBJECT */
#define	MAX_OID_LEN		32      /* OBJECT */

/*
 * Protocol header prefixed to all messages
 */
struct agentx_hdr {
	uint8_t		version;
	uint8_t		type;
	uint8_t		flags;
	uint8_t		reserved;
	uint32_t	sessionid;	/* chosen by agent */
	uint32_t	transactid;	/* chosen by subagent */
	uint32_t	packetid;	/* per-request id */
	uint32_t	length;
} __packed;

/*
 * Prefixed to a series of 4-byte values indicating the OID
 */
struct agentx_oid_hdr {
	uint8_t		n_subid;	/* # of oid elements (named in RFC) */
	uint8_t		prefix;		/* if not 0, OID is 1.3.6.1.<prefix> */
	uint8_t		include;	/* is OID included in search range */
	uint8_t		reserved;	/* always 0 */
} __packed;

struct agentx_response_data {
	uint32_t	sysuptime;	/* uptime of SNMP context */
	uint16_t	error;		/* status of request */
	uint16_t	index;		/* index of failed variable binding */
} __packed;

struct agentx_open_timeout {
	uint8_t		timeout;
	uint8_t		reserved[3];
} __packed;

struct agentx_register_hdr {
	uint8_t		timeout;
	uint8_t		priority;
	uint8_t		subrange;
	uint8_t		reserved;
} __packed;

struct agentx_unregister_hdr {
	uint8_t		reserved1;
	uint8_t		priority;
	uint8_t		subrange;
	uint8_t		reserved2;
} __packed;

struct agentx_null_oid {
	uint8_t		padding[4];
} __packed;

#define	AGENTX_NULL_OID	{ 0, 0, 0, 0 }

struct agentx_varbind_hdr {
	uint16_t	type;
	uint16_t	reserved;
} __packed;

struct agentx_response {
	struct agentx_hdr		hdr;
	struct agentx_response_data	data;
} __packed;

struct agentx_close_request_data {
	uint8_t			reason;
	uint8_t			padding[3];
} __packed;

struct agentx_close_request {
	struct agentx_hdr		hdr;
	struct agentx_close_request_data data;
} __packed;

struct agentx_getbulk_repeaters {
	uint16_t		nonrepeaters;
	uint16_t		maxrepetitions;
} __packed;

struct agentx_pdu {
	uint8_t		*buffer;
	uint8_t		*ptr;
	uint8_t		*ioptr;
	size_t		 buflen;
	size_t		 datalen;
	struct agentx_hdr *hdr;

	char		*context;
	uint32_t	 contextlen;

	void		  *cookie;
	struct agentx_pdu *request;	/* request this is a response to */
	TAILQ_ENTRY(agentx_pdu) entry;
};
TAILQ_HEAD(agentx_pdulist, agentx_pdu);

struct agentx_handle {
	int		 fd;
	uint32_t	 sessionid;
	uint32_t	 transactid;
	uint32_t	 packetid;
	int		 timeout;	/* in seconds */
	int		 error;
	int		 erridx;

	struct agentx_pdulist w;
	struct agentx_pdulist inflight;

	struct agentx_pdu *r;
};

struct agentx_search_range {
	struct snmp_oid	start;
	struct snmp_oid	end;
	int		include; /* is start oid included in search range */
};

struct agentx_handle *
	snmp_agentx_alloc(int);
struct agentx_handle *
	snmp_agentx_open(const char *, char *, struct snmp_oid *);
struct agentx_handle *
	snmp_agentx_fdopen(int, char *, struct snmp_oid *);
int	snmp_agentx_response(struct agentx_handle *, struct agentx_pdu *);
int	snmp_agentx_open_response(struct agentx_handle *, struct agentx_pdu *);
struct agentx_pdu *
	snmp_agentx_open_pdu(struct agentx_handle *, char *descr,
	    struct snmp_oid *);
struct agentx_pdu *
	snmp_agentx_close_pdu(struct agentx_handle *, uint8_t);
int	snmp_agentx_close(struct agentx_handle *, uint8_t);
void	snmp_agentx_free(struct agentx_handle *);
int	snmp_agentx_ping(struct agentx_handle *);
struct agentx_pdu *
	snmp_agentx_ping_pdu(void);
struct agentx_pdu *
	snmp_agentx_notify_pdu(struct snmp_oid *);
struct agentx_pdu *
	snmp_agentx_request(struct agentx_handle *, struct agentx_pdu *);
int	snmp_agentx_varbind(struct agentx_pdu *, struct snmp_oid *, int,
	    void *, int);
int	snmp_agentx_send(struct agentx_handle *, struct agentx_pdu *);
int	snmp_agentx_enqueue(struct agentx_handle *, struct agentx_pdu *);
int	snmp_agentx_flush(struct agentx_handle *);
struct agentx_pdu *
	snmp_agentx_recv(struct agentx_handle *);
struct agentx_pdu *
	snmp_agentx_response_pdu(int, int, int);
struct agentx_pdu *
	snmp_agentx_register_pdu(struct snmp_oid *, int, int, int);
struct agentx_pdu *
	snmp_agentx_unregister_pdu(struct snmp_oid *, int, int);
struct agentx_pdu *
	snmp_agentx_get_pdu(struct snmp_oid *, int);
struct agentx_pdu *
	snmp_agentx_getnext_pdu(struct snmp_oid *, int);
char	*snmp_agentx_read_octetstr(struct agentx_pdu *, int *);
int	snmp_agentx_read_oid(struct agentx_pdu *, struct snmp_oid *);
int	snmp_agentx_read_searchrange(struct agentx_pdu *,
	    struct agentx_search_range *);
int	snmp_agentx_read_raw(struct agentx_pdu *, void *, int);
int	snmp_agentx_copy_raw(struct agentx_pdu *, void *, int);
char	*snmp_agentx_type2name(int);
int	snmp_agentx_read_int(struct agentx_pdu *, uint32_t *);
int	snmp_agentx_read_int64(struct agentx_pdu *, uint64_t *);
int	snmp_agentx_raw(struct agentx_pdu *, void *, int);
int	snmp_agentx_read_vbhdr(struct agentx_pdu *, struct
	    agentx_varbind_hdr *);
struct agentx_pdu *snmp_agentx_pdu_alloc(void);
void	snmp_agentx_pdu_free(struct agentx_pdu *);
char	*snmp_oid2string(struct snmp_oid *, char *, size_t);
int	snmp_oid_cmp(struct snmp_oid *, struct snmp_oid *);
void	snmp_oid_increment(struct snmp_oid *);

#if BYTE_ORDER == BIG_ENDIAN

static __inline int
snmp_agentx_byteorder_native(struct agentx_hdr *h)
{
	return ((h->flags & AGENTX_NETWORK_BYTE_ORDER) != 0);
}

#define AGENTX_LOCAL_BYTE_ORDER_FLAG AGENTX_NETWORK_BYTE_ORDER
#define snmp_agentx_int_byteswap(_i)	htole32(_i)
#define snmp_agentx_int16_byteswap(_i)	htole16(_i)
#define snmp_agentx_int64_byteswap(_i)	htole64(_i)

#elif BYTE_ORDER == LITTLE_ENDIAN

static __inline int
snmp_agentx_byteorder_native(struct agentx_hdr *h)
{
	return ((h->flags & AGENTX_NETWORK_BYTE_ORDER) == 0);
}

#define AGENTX_LOCAL_BYTE_ORDER_FLAG 0
#define snmp_agentx_int_byteswap(_i)	htobe32(_i)
#define snmp_agentx_int16_byteswap(_i)	htobe16(_i)
#define snmp_agentx_int64_byteswap(_i)	htobe64(_i)

#else
#error "Unknown host byte order"
#endif

#endif /* SNMPD_SNMP_H */
