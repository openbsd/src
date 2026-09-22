/*	$OpenBSD: rtrd.h,v 1.2 2026/09/22 01:14:18 rcovelli Exp $	*/

/*
 * Copyright (c) 2025-2026 Ralph Covelli <rcovelli@he.net>
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

#define RTR_VERSION_0	0
#define RTR_VERSION_1	1
#define RTR_VERSION_2	2

#define RTR_DEFAULT_VERSION	RTR_VERSION_0
#define RTR_MAX_VERSION		RTR_VERSION_2

/*
 * Stats
 */

#define HOSTNAME_SIZE	(NI_MAXHOST+1)
#define NODENAME_SIZE	256
#define DOMAINNAME_SIZE	256
#define RELEASE_SIZE	64

struct global_stats
{
	time_t start_time;
	int64_t total_bytes_in;
	int64_t total_bytes_out;
	int64_t total_client_connects;
	int64_t total_controller_connects;
	char nodename[NODENAME_SIZE];
	char domainname[DOMAINNAME_SIZE];
	char release[RELEASE_SIZE];
};

struct socket_stats
{
	time_t connect_time;
	int64_t total_bytes_in;
	int64_t total_bytes_out;
	int64_t vrp4_announcements;
	int64_t vrp4_withdrawals;
	int64_t vrp4_advertised;
	int64_t vrp6_announcements;
	int64_t vrp6_withdrawals;
	int64_t vrp6_advertised;
	int64_t brk_announcements;
	int64_t brk_withdrawals;
	int64_t brk_advertised;
	int64_t vap_announcements;
	int64_t vap_withdrawals;
	int64_t vap_advertised;
	int64_t reset_query_count;
	int64_t serial_query_count;
};

#define STATS_OVERFLOW	-1

#define addstats(stat, val)               \
do {                                      \
	if ((stat) > STATS_OVERFLOW)      \
		(stat) += (val);          \
	if ((stat) < STATS_OVERFLOW)      \
		(stat) = STATS_OVERFLOW;  \
} while (0)

#define substats(stat, val)               \
do {                                      \
	if ((stat) > STATS_OVERFLOW)      \
		(stat) -= (val);          \
	if ((stat) < STATS_OVERFLOW)      \
		(stat) = STATS_OVERFLOW;  \
} while (0)

#define PACKED __attribute__((packed))

/*
 * Tables
 */

/* RPKI Objects */

/* Validated ROA4 Payload (VRP4) */

struct vrp4
{
	RB_ENTRY(vrp4) entry;
	time_t expire;
	uint8_t prefix_length;
	uint8_t max_prefix_length;
	uint16_t zero;
	struct in_addr prefix;
	uint32_t asn;
} PACKED;

RB_HEAD(vrp4_tree, vrp4);
RB_PROTOTYPE(vrp4_tree, vrp4, entry, vrp4cmp)

/* Validated ROA6 Payload (VRP6) */

struct vrp6
{
	RB_ENTRY(vrp6) entry;
	time_t expire;
	uint8_t prefix_length;
	uint8_t max_prefix_length;
	uint16_t zero;
	struct in6_addr prefix;
	uint32_t asn;
} PACKED;

RB_HEAD(vrp6_tree, vrp6);
RB_PROTOTYPE(vrp6_tree, vrp6, entry, vrp6cmp)

/* BGPsec Router Key (BRK) */

#define SKI_LENGTH 20
#define SPKI_LENGTH_P256 91

struct brk
{
	RB_ENTRY(brk) entry;
	time_t expire;
	unsigned char ski[SKI_LENGTH];  /* Subject Key Identifier */
	uint32_t asn;
	uint32_t spki_length;
	unsigned char spki[];  /* Subject Public Key Info */
} PACKED;

RB_HEAD(brk_tree, brk);
RB_PROTOTYPE(brk_tree, brk, entry, brkcmp)

/* Validated ASPA Payload (VAP) */

/* PDU ASPA max providers 16380 */
#define VAP_MAX_PROVIDERS	4096

struct vap
{
	RB_ENTRY(vap) entry;
	time_t expire;
	uint32_t customer_asn;
	int32_t provider_count;
	uint32_t provider_asns[];
} PACKED;

struct vap_buffer
{
	RB_ENTRY(vap) entry;
	time_t expire;
	uint32_t customer_asn;
	int32_t provider_count;
	uint32_t provider_asns[VAP_MAX_PROVIDERS];
} PACKED;

RB_HEAD(vap_tree, vap);
RB_PROTOTYPE(vap_tree, vap, entry, vapcmp)

struct asn
{
	RB_ENTRY(asn) entry;
	uint32_t asn;
} PACKED;

RB_HEAD(asn_tree, asn);
RB_PROTOTYPE(asn_tree, asn, entry, asncmp)

/*
 * Sockets
 */

#define PDU_MAX_LENGTH	65535

#define RTR_SOCKET_TYPE_UNKNOWN		0
#define RTR_SOCKET_TYPE_CLIENT		1
#define RTR_SOCKET_TYPE_CONTROLLER	2

#define RTR_SOCKET_NAME_LENGTH		128

#define RTR_SOCKET_FLAG_INUSE           0x00000001U
#define RTR_SOCKET_FLAG_CLOSED          0x00000002U
#define RTR_SOCKET_FLAG_REGISTERED      0x00000004U

#define  TestSocketFlag(s, f)   (((s)->flags) &    (f))
#define   SetSocketFlag(s, f)   (((s)->flags) |=   (f))
#define ClearSocketFlag(s, f)   (((s)->flags) &= (~(f)))

#define POLLEVENT(i) (poll_table[(i)].events)

struct rtr_sendq_link
{
	ssize_t length;
	ssize_t offset;
	struct rtr_sendq_link *next;
	unsigned char sendq_block[];
};

struct rtr_sendq
{
	struct rtr_sendq_link *head;
	struct rtr_sendq_link *tail;
	ssize_t length;
	ssize_t max;
};

struct rtr_socket
{
	int fd;
	int type;
	char *name;
	uint32_t flags;
	struct sockaddr_in client;
	unsigned char *read_pdu;
	ssize_t read_length;
	ssize_t read_block_size;
	unsigned char *write_block;
	ssize_t write_length;
	ssize_t write_block_size;
	struct rtr_sendq sendq;
	uint8_t version;
	struct vrp4_tree vrp4s;
	struct vrp6_tree vrp6s;
	struct brk_tree brks;
	struct vap_tree vaps;
	struct socket_stats stats;
};

/*
 * Protocol Data Units
 */

enum pdu_type {
	SERIAL_NOTIFY	=  0,
	SERIAL_QUERY	=  1,
	RESET_QUERY	=  2,
	CACHE_RESPONSE	=  3,
	IPV4_PREFIX	=  4,
	RESERVED	=  5,
	IPV6_PREFIX	=  6,
	END_OF_DATA	=  7,
	CACHE_RESET	=  8,
	ROUTER_KEY	=  9,
	ERROR		= 10,
	ASPA_PDU	= 11,
	PDU_TYPE_MAX	= 12,
	OPEN_CONTROLLER		= 128,
	CLOSE_CONTROLLER	= 129,
	START_OF_IMPORT		= 130,
	IPV4_PREFIX_IMPORT	= 131,
	IPV6_PREFIX_IMPORT	= 132,
	ROUTER_KEY_IMPORT	= 133,
	ASPA_PDU_IMPORT		= 134,
	END_OF_IMPORT		= 135,
	PUSH_IMPORT		= 136,
	QUERY_STATS		= 137,
	START_OF_STATS		= 138,
	GLOBAL_STATS		= 139,
	CLIENT_STATS		= 140,
	CACHE_FRAME_STATS	= 141,
	END_OF_STATS		= 142,
	PDU_RTRX_MAX
};

#define PDU_SPLIT	128

#define RTR_WITHDRAW	0x00
#define RTR_ANNOUNCE	0x01

struct pdu_header
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

struct pdu_serial_notify
{
	uint8_t version;
	uint8_t type;
	uint16_t session_id;
	uint32_t length;
	uint32_t serial_number;
} PACKED;

struct pdu_serial_query
{
	uint8_t version;
	uint8_t type;
	uint16_t session_id;
	uint32_t length;
	uint32_t serial_number;
} PACKED;

struct pdu_reset_query
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

struct pdu_cache_response
{
	uint8_t version;
	uint8_t type;
	uint16_t session_id;
	uint32_t length;
} PACKED;

struct pdu_ipv4_prefix
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
	uint8_t flags;
	uint8_t prefix_length;
	uint8_t max_prefix_length;
	uint8_t zero;
	struct in_addr prefix;
	uint32_t asn;
} PACKED;

struct pdu_ipv6_prefix
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
	uint8_t flags;
	uint8_t prefix_length;
	uint8_t max_prefix_length;
	uint8_t zero;
	struct in6_addr prefix;
	uint32_t asn;
} PACKED;

struct pdu_end_of_data_v0
{
	uint8_t version;
	uint8_t type;
	uint16_t session_id;
	uint32_t length;
	uint32_t serial_number;
} PACKED;

struct pdu_end_of_data
{
	uint8_t version;
	uint8_t type;
	uint16_t session_id;
	uint32_t length;
	uint32_t serial_number;
	uint32_t refresh_interval;
	uint32_t retry_interval;
	uint32_t expire_interval;
} PACKED;

struct pdu_cache_reset
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

struct pdu_router_key
{
	uint8_t version;
	uint8_t type;
	uint8_t flags;
	uint8_t zero;
	uint32_t length;
	unsigned char ski[SKI_LENGTH];
	uint32_t asn;
	unsigned char spki[];
} PACKED;

enum pdu_error_code {
	CORRUPT_DATA			=  0,
	INTERNAL_ERROR			=  1,
	NO_DATA_AVAILABLE		=  2,
	INVALID_REQUEST			=  3,
	UNSUPPORTED_PROTOCOL_VERSION	=  4,
	UNSUPPORTED_PDU_TYPE		=  5,
	WITHDRAWAL_OF_UNKNOWN_RECORD	=  6,
	DUPLICATE_ANNOUNCEMENT_RECEIVED	=  7,
	UNEXPECTED_PROTOCOL_VERSION	=  8,
	ASPA_PROVIDER_LIST_ERROR	=  9,
	TRANSPORT_ERROR			= 10,
	ORDERING_ERROR			= 11,
	CACHE_RESTART			= 12,
	CACHE_SHUTDOWN			= 13,
	PDU_ERROR_MAX
};

struct pdu_error
{
	uint8_t version;
	uint8_t type;
	uint16_t error_code;
	uint32_t length;
	uint32_t pdu_length;
	unsigned char pdu[];
	/* uint32_t text_length; */
	/* char text[]; */
} PACKED;

struct pdu_aspa
{
	uint8_t version;
	uint8_t type;
	uint8_t flags;
	uint8_t zero;
	uint32_t length;
	uint32_t customer_asn;
	uint32_t provider_asns[];
} PACKED;

/*
 * Controller PDUs
 */

struct pdu_open_controller
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
	uint32_t controller_version;
	uint32_t controller_flags;
} PACKED;

struct pdu_close_controller
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

struct pdu_start_of_import
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

struct pdu_ipv4_prefix_import
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
	time_t expire;
	uint8_t flags;
	uint8_t prefix_length;
	uint8_t max_prefix_length;
	uint8_t zero;
	struct in_addr prefix;
	uint32_t asn;
} PACKED;

struct pdu_ipv6_prefix_import
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
	time_t expire;
	uint8_t flags;
	uint8_t prefix_length;
	uint8_t max_prefix_length;
	uint8_t zero;
	struct in6_addr prefix;
	uint32_t asn;
} PACKED;

struct pdu_router_key_import
{
	uint8_t version;
	uint8_t type;
	uint8_t flags;
	uint8_t zero;
	uint32_t length;
	time_t expire;
	unsigned char ski[SKI_LENGTH];
	uint32_t asn;
	unsigned char spki[];
} PACKED;

struct pdu_aspa_import
{
	uint8_t version;
	uint8_t type;
	uint8_t flags;
	uint8_t zero;
	uint32_t length;
	time_t expire;
	uint32_t customer_asn;
	uint32_t provider_asns[];
} PACKED;

struct pdu_end_of_import
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

struct pdu_push_import
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

struct pdu_query_stats
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

struct pdu_start_of_stats
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

struct pdu_global_stats
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
	char nodename[NODENAME_SIZE];
	char domainname[DOMAINNAME_SIZE];
	char release[RELEASE_SIZE];
	time_t start_time;
	int64_t total_bytes_in;
	int64_t total_bytes_out;
	int64_t total_client_connects;
	int64_t total_controller_connects;
} PACKED;

#define CLIENT_STATE_REGISTERED	0x01
#define CLIENT_STATE_CLOSED	0x02

struct pdu_client_stats
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
	int64_t fd;
	struct in_addr client_ip;
	uint16_t client_port;
	uint8_t client_version;
	uint8_t client_state;
	time_t connect_time;
	int64_t total_bytes_in;
	int64_t total_bytes_out;
	int64_t sendq_length;
	int64_t sendq_max;
	int64_t vrp4_announcements;
	int64_t vrp4_withdrawals;
	int64_t vrp4_advertised;
	int64_t vrp6_announcements;
	int64_t vrp6_withdrawals;
	int64_t vrp6_advertised;
	int64_t brk_announcements;
	int64_t brk_withdrawals;
	int64_t brk_advertised;
	int64_t vap_announcements;
	int64_t vap_withdrawals;
	int64_t vap_advertised;
	int64_t reset_query_count;
	int64_t serial_query_count;
} PACKED;

#define CACHE_FRAME_FLAGS_ACTIVE	0x01

struct pdu_cache_frame_stats
{
	uint8_t version;
	uint8_t type;
	uint8_t reserved;
	uint8_t flags;
	uint32_t length;
	uint8_t cache_version;
	uint8_t zero;
	uint16_t cache_session_id;
	uint32_t cache_serial;
	int64_t vrp4_count;
	time_t vrp4_creation_time;
	int64_t vrp6_count;
	time_t vrp6_creation_time;
	int64_t brk_count;
	time_t brk_creation_time;
	int64_t vap_count;
	time_t vap_creation_time;
} PACKED;

struct pdu_end_of_stats
{
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

/*
 * Cache
 */

struct cache_vrp4_tree
{
	struct vrp4_tree vrp4s;
	int reference_count;
	uint32_t hash;
	int64_t entry_count;
	time_t creation_time;
};

struct cache_vrp6_tree
{
	struct vrp6_tree vrp6s;
	int reference_count;
	uint32_t hash;
	int64_t entry_count;
	time_t creation_time;
};

struct cache_brk_tree
{
	struct brk_tree brks;
	int reference_count;
	uint32_t hash;
	int64_t entry_count;
	time_t creation_time;
};

struct cache_vap_tree
{
	struct vap_tree vaps;
	int reference_count;
	uint32_t hash;
	int64_t entry_count;
	time_t creation_time;
};

struct cache_frame
{
	uint32_t serial_number;
	struct cache_vrp4_tree *rtr_vrp4s;
	struct cache_vrp6_tree *rtr_vrp6s;
	struct cache_brk_tree *rtr_brks;
	struct cache_vap_tree *rtr_vaps;
};

struct cache
{
	struct cache_frame *frames;
	int head;
	int tail;
	uint32_t next_serial_number;
	uint32_t refresh_interval;
	uint32_t retry_interval;
	uint32_t expire_interval;
	uint16_t session_id;
	uint16_t frame_count;
};

#define CHEAD(v) (cache[(v)].frames[cache[(v)].head])
#define CFRAME(v,i) (cache[(v)].frames[(i)])

/*
 * Scheduler
 */

#define SCHED_TYPE_CLEANUP		0
#define SCHED_TYPE_HANDSHAKE_TIMEOUT	1
#define SCHED_TYPE_IDLE_TIMEOUT		2

struct sched
{
	RB_ENTRY(sched) entry;
	time_t event_time;
	uint32_t type;
	struct rtr_socket *rtr_socket;
} PACKED;

RB_HEAD(sched_tree, sched);
RB_PROTOTYPE(sched_tree, sched, entry, schedcmp)

/* VRP4 */

extern RB_PROTOTYPE(vrp4_tree, vrp4, entry, vrp4cmp)

extern int insert_vrp4(struct vrp4_tree *, struct vrp4 *, time_t);
extern int remove_vrp4(struct vrp4_tree *, struct vrp4 *);
extern void free_vrp4_tree(struct vrp4_tree *);
extern int vrp4_treecmp(struct vrp4_tree *, struct vrp4_tree *);
extern void vrp4_treecpy(struct vrp4_tree *, struct vrp4_tree *);
extern void vrp4_treecpy_expire(struct vrp4_tree *, struct vrp4_tree *, time_t);
extern int64_t vrp4_treecount(struct vrp4_tree *);
extern int vrp4_treeempty(struct vrp4_tree *);
extern void vrp4_treeupdate(struct vrp4_tree *, struct vrp4_tree *);
extern uint32_t vrp4_treehash(struct vrp4_tree *);
extern void vrp4_treecpy_count_and_hash(
    struct vrp4_tree *,
    struct vrp4_tree *,
    int64_t *,
    uint32_t *);

extern struct cache_vrp4_tree *cache_vrp4_tree_find(struct vrp4_tree *);
extern struct cache_vrp4_tree *cache_vrp4_tree_dup(struct cache_vrp4_tree *);
extern struct cache_vrp4_tree *cache_vrp4_tree_new(struct vrp4_tree *);
extern void cache_vrp4_tree_free(struct cache_vrp4_tree *);

/* VRP6 */

extern RB_PROTOTYPE(vrp6_tree, vrp6, entry, vrp6cmp)

extern int insert_vrp6(struct vrp6_tree *, struct vrp6 *, time_t);
extern int remove_vrp6(struct vrp6_tree *, struct vrp6 *);
extern void free_vrp6_tree(struct vrp6_tree *);
extern int vrp6_treecmp(struct vrp6_tree *, struct vrp6_tree *);
extern void vrp6_treecpy(struct vrp6_tree *, struct vrp6_tree *);
extern void vrp6_treecpy_expire(struct vrp6_tree *, struct vrp6_tree *, time_t);
extern int64_t vrp6_treecount(struct vrp6_tree *);
extern int vrp6_treeempty(struct vrp6_tree *);
extern void vrp6_treeupdate(struct vrp6_tree *, struct vrp6_tree *);
extern uint32_t vrp6_treehash(struct vrp6_tree *);
extern void vrp6_treecpy_count_and_hash(
    struct vrp6_tree *,
    struct vrp6_tree *,
    int64_t *,
    uint32_t *);

extern struct cache_vrp6_tree *cache_vrp6_tree_find(struct vrp6_tree *);
extern struct cache_vrp6_tree *cache_vrp6_tree_dup(struct cache_vrp6_tree *);
extern struct cache_vrp6_tree *cache_vrp6_tree_new(struct vrp6_tree *);
extern void cache_vrp6_tree_free(struct cache_vrp6_tree *);

/* BRK */

extern RB_PROTOTYPE(brk_tree, brk, entry, brkcmp)

extern int insert_brk(struct brk_tree *, struct brk *, time_t);
extern int remove_brk(struct brk_tree *, struct brk *);
extern void free_brk_tree(struct brk_tree *);
extern int brk_treecmp(struct brk_tree *, struct brk_tree *);
extern void brk_treecpy(struct brk_tree *, struct brk_tree *);
extern void brk_treecpy_expire(struct brk_tree *, struct brk_tree *, time_t);
extern int64_t brk_treecount(struct brk_tree *);
extern int brk_treeempty(struct brk_tree *);
extern void brk_treeupdate(struct brk_tree *, struct brk_tree *);
extern uint32_t brk_treehash(struct brk_tree *);
extern void brk_treecpy_count_and_hash(
    struct brk_tree *,
    struct brk_tree *,
    int64_t *,
    uint32_t *);

extern struct cache_brk_tree *cache_brk_tree_find(struct brk_tree *);
extern struct cache_brk_tree *cache_brk_tree_dup(struct cache_brk_tree *);
extern struct cache_brk_tree *cache_brk_tree_new(struct brk_tree *);
extern void cache_brk_tree_free(struct cache_brk_tree *);

/* ASN */

extern RB_PROTOTYPE(asn_tree, asn, entry, asncmp)

extern int insert_asn(struct asn_tree *, uint32_t);
extern int remove_asn(struct asn_tree *, uint32_t);
extern void free_asn_tree(struct asn_tree *);
extern int clean_asn_tree(struct asn_tree *);
extern int32_t asn_treecount(struct asn_tree *);

/* VAP */

extern RB_PROTOTYPE(vap_tree, vap, entry, vapcmp)

extern int vapfullcmp(struct vap *, struct vap *);
extern int insert_vap(struct vap_tree *, struct vap *, time_t);
extern int remove_vap(struct vap_tree *, struct vap *);
extern void free_vap_tree(struct vap_tree *);
extern int vap_treecmp(struct vap_tree *, struct vap_tree *);
extern void vap_treecpy(struct vap_tree *, struct vap_tree *);
extern void vap_treecpy_expire(struct vap_tree *, struct vap_tree *, time_t);
extern int64_t vap_treecount(struct vap_tree *);
extern int vap_treeempty(struct vap_tree *);
extern void vap_treeupdate(struct vap_tree *, struct vap_tree *);
extern uint32_t vap_treehash(struct vap_tree *);
extern void vap_treecpy_count_and_hash(
    struct vap_tree *,
    struct vap_tree *,
    int64_t *,
    uint32_t *);

extern struct cache_vap_tree *cache_vap_tree_find(struct vap_tree *);
extern struct cache_vap_tree *cache_vap_tree_dup(struct cache_vap_tree *);
extern struct cache_vap_tree *cache_vap_tree_new(struct vap_tree *);
extern void cache_vap_tree_free(struct cache_vap_tree *);

extern void init_cache_frame(struct cache_frame *);
extern int init_cache(struct cache *, uint16_t, uint16_t);
extern int init_cache_array(uint16_t);
extern int is_cache_empty(struct cache *);
extern void free_cache_frame(struct cache_frame *);
extern struct cache_frame *get_latest_cache_frame(uint8_t);
extern struct cache_frame *get_serial_cache_frame(uint8_t, uint32_t);
extern struct cache_frame *cache_frame_push(uint8_t,
    struct cache_vrp4_tree *, struct cache_vrp6_tree *,
    struct cache_brk_tree *, struct cache_vap_tree *);
extern void update_cache(struct vrp4_tree *, struct vrp6_tree *,
    struct brk_tree *, struct vap_tree *);

extern struct cache cache[];

extern int m_serial_notify(struct rtr_socket *, struct pdu_header *);
extern int m_serial_query(struct rtr_socket *, struct pdu_header *);
extern int m_reset_query(struct rtr_socket *, struct pdu_header *);
extern int m_cache_response(struct rtr_socket *, struct pdu_header *);
extern int m_ipv4_prefix(struct rtr_socket *, struct pdu_header *);
extern int m_reserved(struct rtr_socket *, struct pdu_header *);
extern int m_ipv6_prefix(struct rtr_socket *, struct pdu_header *);
extern int m_end_of_data(struct rtr_socket *, struct pdu_header *);
extern int m_cache_reset(struct rtr_socket *, struct pdu_header *);
extern int m_router_key(struct rtr_socket *, struct pdu_header *);
extern int m_error(struct rtr_socket *, struct pdu_header *);
extern int m_aspa_pdu(struct rtr_socket *, struct pdu_header *);

extern int m_open_controller(struct rtr_socket *, struct pdu_header *);
extern int m_close_controller(struct rtr_socket *, struct pdu_header *);
extern int m_start_of_import(struct rtr_socket *, struct pdu_header *);
extern int m_ipv4_prefix_import(struct rtr_socket *, struct pdu_header *);
extern int m_ipv6_prefix_import(struct rtr_socket *, struct pdu_header *);
extern int m_router_key_import(struct rtr_socket *, struct pdu_header *);
extern int m_aspa_pdu_import(struct rtr_socket *, struct pdu_header *);
extern int m_end_of_import(struct rtr_socket *, struct pdu_header *);
extern int m_push_import(struct rtr_socket *, struct pdu_header *);
extern int m_query_stats(struct rtr_socket *, struct pdu_header *);
extern int m_start_of_stats(struct rtr_socket *, struct pdu_header *);
extern int m_global_stats(struct rtr_socket *, struct pdu_header *);
extern int m_client_stats(struct rtr_socket *, struct pdu_header *);
extern int m_cache_frame_stats(struct rtr_socket *, struct pdu_header *);
extern int m_end_of_stats(struct rtr_socket *, struct pdu_header *);

extern int (*command_lookup(uint8_t, uint8_t, int))
    (struct rtr_socket *, struct pdu_header *);

extern uint32_t fnv32_init;
extern uint32_t fnv32_hash(void *, size_t, uint32_t);

extern struct in_addr cidrmask4[];
extern struct in6_addr cidrmask6[];

extern void init_masks(void);
extern struct in_addr cidr_to_netmask4(uint8_t);
extern struct in6_addr cidr_to_netmask6(uint8_t);
extern int is_supernet_of_subnet4(
    struct in_addr, uint8_t,
    struct in_addr, uint8_t);
extern int is_supernet_of_subnet6(
    struct in6_addr, uint8_t,
    struct in6_addr, uint8_t);

extern struct in_addr address_to_network4(struct in_addr, uint8_t);
extern struct in6_addr address_to_network6(struct in6_addr, uint8_t);

extern int verbose;
extern int foreground;

extern void logx(int, const char *, ...);

extern char *error_code_to_str[];
extern char *pdu_type_to_str[];

extern void pdu_hton(struct rtr_socket *, void *);
extern void pdu_ntoh(struct rtr_socket *, void *);
extern void pdu_hton_rtr(void *);
extern void pdu_ntoh_rtr(void *);
extern void pdu_hton_controller(void *);
extern void pdu_ntoh_controller(void *);
extern int check_pdu_ipv4_prefix_controller(struct pdu_ipv4_prefix_import *);
extern int check_pdu_ipv6_prefix_controller(struct pdu_ipv6_prefix_import *);
extern int check_pdu_router_key_controller(struct pdu_router_key_import *);
extern int check_pdu_aspa_controller(struct pdu_aspa_import *);
extern int check_error_code(uint16_t, uint8_t);

extern struct timeval now;

extern void rtr_gettime(void);
extern RB_PROTOTYPE(sched_tree, sched, entry, schedcmp)
extern int insert_sched(struct sched_tree *, struct sched *);
extern int remove_sched(struct sched_tree *, struct sched *);
extern void remove_sched_type(struct sched_tree *, uint32_t);
extern void remove_sched_socket(struct sched_tree *, struct rtr_socket *);
extern void remove_sched_type_and_socket(
    struct sched_tree *,
    uint32_t,
    struct rtr_socket *);
extern void free_sched_tree(struct sched_tree *);
extern void sched_init(void);
extern int time_until_next_event(struct timeval);
extern int schedule_cleanup(time_t);
extern int schedule_handshake_timeout(time_t, struct rtr_socket *);
extern int schedule_idle_timeout(time_t, struct rtr_socket *);
extern void process_cleanup(time_t);
extern void process_handshake_timeout(struct rtr_socket *);
extern void process_idle_timeout(struct rtr_socket *);
extern void process_scheduler_events(time_t);

extern struct sched_tree scheduler;

extern ssize_t sendto_one(struct rtr_socket *, void *);
extern void sendto_allclients(void *);
extern void sendto_allregisteredclients(void *);
extern void sendto_allregisteredclientsversion(void *, uint8_t);

extern ssize_t sendserialnotifyto_one(struct rtr_socket *, uint32_t);
extern ssize_t sendcacheresponseto_one(struct rtr_socket *);
extern ssize_t sendvrp4to_one(struct rtr_socket *, struct vrp4 *, uint8_t);
extern ssize_t sendvrp6to_one(struct rtr_socket *, struct vrp6 *, uint8_t);
extern ssize_t sendbrkto_one(struct rtr_socket *, struct brk *, uint8_t);
extern ssize_t sendvapto_one(struct rtr_socket *, struct vap *, uint8_t);
extern ssize_t sendendofdatato_one(struct rtr_socket *, uint32_t);
extern ssize_t senderrorto_one(struct rtr_socket *, uint16_t, void *, char *,
    ...);
extern ssize_t sendcacheresetto_one(struct rtr_socket *);

extern ssize_t sendstartofstatsto_one(struct rtr_socket *);
extern ssize_t sendglobalstatsto_one(struct rtr_socket *);
extern void sendclientstatsto_one(struct rtr_socket *);
extern void sendcacheframestatsto_one(struct rtr_socket *);
extern ssize_t sendendofstatsto_one(struct rtr_socket *);

#define sigflags (sigterm || sigint || sigquit || sigusr1 || sighup)

extern volatile sig_atomic_t sigterm;
extern volatile sig_atomic_t sigint;
extern volatile sig_atomic_t sigquit;
extern volatile sig_atomic_t sigusr1;
extern volatile sig_atomic_t sighup;

extern int init_signals(void);
extern void signal_handler(int);
extern void signal_processor(void);

extern int listener;
extern int controller;
extern char *controller_filename;
extern int client_count;
extern int max_clients;
extern int max_sockets;
extern int max_sendq;

extern struct rtr_socket *rtr_socket_table;

extern struct pollfd *poll_table;
extern int poll_table_count;

extern uint8_t rtr_max_version;

extern ssize_t sendq_block_size;

extern struct rtr_socket *fd_to_socket(int);
extern int is_listener(int);
extern struct pollfd *poll_find(int);
extern int poll_index(int);
extern struct pollfd *poll_add(int, short);
extern void poll_remove(int);
extern int poll_isset_events(int, short);
extern int poll_isset_revents(int, short);
extern void init_socket(int, int, uint32_t, ssize_t, ssize_t, ssize_t, uint8_t);
extern int init_socket_table(FILE *, char *, uint16_t);
extern int rtr_errno_ignore(int);
extern ssize_t rtr_flush_write(struct rtr_socket *);
extern void rtr_flushall_write(void);
extern int rtr_sendq_add(struct rtr_socket *, void *, int);
extern void rtr_sendq_pop(struct rtr_socket *, int);
extern void rtr_sendq_popall(struct rtr_socket *);
extern int rtr_sendq_flush(struct rtr_socket *);
extern ssize_t writeto(struct rtr_socket *, void *);
extern void rtr_close(struct rtr_socket *);
extern void rtr_flushall_closed(void);
extern void rtr_shutdown(int);
extern void core_loop(void);

extern struct global_stats global_stats;

extern int init_stats(void);
