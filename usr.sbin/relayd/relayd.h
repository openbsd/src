/*	$OpenBSD: relayd.h,v 1.201 2014/12/21 00:54:49 guenther Exp $	*/

/*
 * Copyright (c) 2006 - 2014 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2006, 2007 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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

#ifndef _RELAYD_H
#define _RELAYD_H

#include <sys/tree.h>

#include <sys/param.h>		/* MAXHOSTNAMELEN */
#include <netinet/in.h>
#include <limits.h>
#include <imsg.h>
#include <siphash.h>

#ifndef nitems
#define	nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

#define CONF_FILE		"/etc/relayd.conf"
#define RELAYD_SOCKET		"/var/run/relayd.sock"
#define PF_SOCKET		"/dev/pf"
#define RELAYD_USER		"_relayd"
#define RELAYD_ANCHOR		"relayd"
#define RELAYD_SERVERNAME	"OpenBSD relayd"
#define CHECK_TIMEOUT		200
#define CHECK_INTERVAL		10
#define EMPTY_TABLE		UINT_MAX
#define EMPTY_ID		UINT_MAX
#define LABEL_NAME_SIZE		1024
#define TAG_NAME_SIZE		64
#define TABLE_NAME_SIZE		64
#define	RD_TAG_NAME_SIZE	64
#define	RT_LABEL_SIZE		32
#define SRV_NAME_SIZE		64
#define MAX_NAME_SIZE		64
#define SRV_MAX_VIRTS		16
#define TLS_NAME_SIZE		512

#define FD_RESERVE		5

#define RELAY_MAX_SESSIONS	1024
#define RELAY_TIMEOUT		600
#define RELAY_CACHESIZE		-1	/* use default size */
#define RELAY_NUMPROC		3
#define RELAY_MAXPROC		32
#define RELAY_MAXHOSTS		32
#define RELAY_MAXHEADERLENGTH	8192
#define RELAY_STATINTERVAL	60
#define RELAY_BACKLOG		10
#define RELAY_MAXLOOKUPLEVELS	5
#define RELAY_OUTOF_FD_RETRIES	5

#define CONFIG_RELOAD		0x00
#define CONFIG_TABLES		0x01
#define CONFIG_RDRS		0x02
#define CONFIG_RELAYS		0x04
#define CONFIG_PROTOS		0x08
#define CONFIG_ROUTES		0x10
#define CONFIG_RTS		0x20
#define CONFIG_CA_ENGINE	0x40
#define CONFIG_ALL		0xff

#define SMALL_READ_BUF_SIZE	1024
#define ICMP_BUF_SIZE		64

#define SNMP_RECONNECT_TIMEOUT	{ 3, 0 }	/* sec, usec */

#if DEBUG > 1
#define DPRINTF		log_debug
#define DEBUG_CERT	1
#else
#define DPRINTF(x...)	do {} while(0)
#endif

/* Used for DNS request ID randomization */
struct shuffle {
	u_int16_t	 id_shuffle[65536];
	int		 isindex;
};

typedef u_int32_t objid_t;

struct ctl_flags {
	u_int8_t	 cf_opts;
	u_int32_t	 cf_flags;
};

struct ctl_status {
	objid_t		 id;
	int		 up;
	int		 retry_cnt;
	u_long		 check_cnt;
	u_int16_t	 he;
};

struct ctl_id {
	objid_t		 id;
	char		 name[MAX_NAME_SIZE];
};

struct ctl_relaytable {
	objid_t		 id;
	objid_t		 relayid;
	int		 mode;
	u_int32_t	 flags;
};

struct ctl_script {
	objid_t		 host;
	int		 retval;
	struct timeval	 timeout;
	char		 name[MAXHOSTNAMELEN];
	char		 path[MAXPATHLEN];
};

struct ctl_demote {
	char		 group[IFNAMSIZ];
	int		 level;
};

struct ctl_icmp_event {
	struct relayd		*env;
	int			 s;
	int			 af;
	int			 last_up;
	struct event		 ev;
	struct timeval		 tv_start;
};

struct ctl_tcp_event {
	int			 s;
	char			*req;
	struct ibuf		*buf;
	struct host		*host;
	struct table		*table;
	struct timeval		 tv_start;
	struct event		 ev;
	int			(*validate_read)(struct ctl_tcp_event *);
	int			(*validate_close)(struct ctl_tcp_event *);

	SSL			*ssl;	/* libssl object */
};

enum direction {
	RELAY_DIR_INVALID	= -1,
	RELAY_DIR_ANY		=  0,
	RELAY_DIR_REQUEST	=  1,
	RELAY_DIR_RESPONSE	=  2
};

enum tlsreneg_state {
	TLSRENEG_INIT		= 0,	/* first/next negotiation is allowed */
	TLSRENEG_ALLOW		= 1,	/* all (re-)negotiations are allowed */
	TLSRENEG_DENY		= 2,	/* next renegotiation must be denied */
	TLSRENEG_ABORT		= 3	/* the connection should be aborted */
};

struct ctl_relay_event {
	int			 s;
	in_port_t		 port;
	struct sockaddr_storage	 ss;
	struct bufferevent	*bev;
	struct evbuffer		*output;
	struct ctl_relay_event	*dst;
	struct rsession		*con;

	SSL			*ssl;	/* libssl object */

	X509			*tlscert;
	enum tlsreneg_state	 tlsreneg_state;

	off_t			 splicelen;
	off_t			 toread;
	size_t			 headerlen;
	int			 line;
	int			 done;
	int			 timedout;
	enum direction		 dir;

	u_int8_t		*buf;
	int			 buflen;

	/* protocol-specific descriptor */
	void			*desc;
};

enum httpchunk {
	TOREAD_UNLIMITED		= -1,
	TOREAD_HTTP_HEADER		= -2,
	TOREAD_HTTP_CHUNK_LENGTH	= -3,
	TOREAD_HTTP_CHUNK_TRAILER	= -4
};

struct ctl_natlook {
	objid_t			 id;
	int			 proc;

	struct sockaddr_storage	 src;
	struct sockaddr_storage	 dst;
	struct sockaddr_storage	 rsrc;
	struct sockaddr_storage	 rdst;
	in_port_t		 rsport;
	in_port_t		 rdport;
	int			 in;
	int			 proto;
};

struct ctl_bindany {
	objid_t			 bnd_id;
	int			 bnd_proc;

	struct sockaddr_storage	 bnd_ss;
	in_port_t		 bnd_port;
	int			 bnd_proto;
};

struct ctl_keyop {
	objid_t			 cko_id;
	int			 cko_proc;
	int			 cko_flen;
	int			 cko_tlen;
	int			 cko_padding;
};

struct ctl_stats {
	objid_t			 id;
	int			 proc;

	u_int64_t		 interval;
	u_int64_t		 cnt;
	u_int32_t		 tick;
	u_int32_t		 avg;
	u_int32_t		 last;
	u_int32_t		 avg_hour;
	u_int32_t		 last_hour;
	u_int32_t		 avg_day;
	u_int32_t		 last_day;
};

enum key_option {
	KEY_OPTION_NONE		= 0,
	KEY_OPTION_APPEND,
	KEY_OPTION_SET,
	KEY_OPTION_REMOVE,
	KEY_OPTION_HASH,
	KEY_OPTION_LOG
};

enum key_type {
	KEY_TYPE_NONE		= 0,
	KEY_TYPE_COOKIE,
	KEY_TYPE_HEADER,
	KEY_TYPE_PATH,
	KEY_TYPE_QUERY,
	KEY_TYPE_URL,
	KEY_TYPE_MAX
};

struct ctl_kvlen {
	ssize_t		 key;
	ssize_t		 value;
};

struct ctl_rule {
	struct ctl_kvlen kvlen[KEY_TYPE_MAX];
};

enum digest_type {
	DIGEST_NONE		= 0,
	DIGEST_SHA1		= 1,
	DIGEST_MD5		= 2
};

TAILQ_HEAD(kvlist, kv);
RB_HEAD(kvtree, kv);

struct kv {
	char			*kv_key;
	char			*kv_value;

	enum key_type		 kv_type;
	enum key_option		 kv_option;
	enum digest_type	 kv_digest;

#define KV_FLAG_MACRO		 0x01
#define KV_FLAG_INVALID		 0x02
#define KV_FLAG_GLOBBING	 0x04
	u_int8_t		 kv_flags;

	struct kvlist		 kv_children;
	struct kv		*kv_parent;
	TAILQ_ENTRY(kv)		 kv_entry;

	RB_ENTRY(kv)		 kv_node;

	/* A few pointers used by the rule actions */
	struct kv		*kv_match;
	struct kvtree		*kv_matchtree;

	TAILQ_ENTRY(kv)		 kv_match_entry;
	TAILQ_ENTRY(kv)		 kv_rule_entry;
	TAILQ_ENTRY(kv)		 kv_action_entry;
};

struct portrange {
	in_port_t		 val[2];
	u_int8_t		 op;
};

struct address {
	objid_t			 rdrid;
	struct sockaddr_storage	 ss;
	int			 ipproto;
	struct portrange	 port;
	char			 ifname[IFNAMSIZ];
	TAILQ_ENTRY(address)	 entry;
};
TAILQ_HEAD(addresslist, address);

union hashkey {
	/* Simplified version of pf_poolhashkey */
	u_int32_t		 data[4];
	SIPHASH_KEY		 siphashkey;
};

#define F_DISABLE		0x00000001
#define F_BACKUP		0x00000002
#define F_USED			0x00000004
#define F_DOWN			0x00000008
#define F_ADD			0x00000010
#define F_DEL			0x00000020
#define F_CHANGED		0x00000040
#define F_STICKY		0x00000080
#define F_CHECK_DONE		0x00000100
#define F_ACTIVE_RULESET	0x00000200
#define F_CHECK_SENT		0x00000400
#define F_TLS			0x00000800
#define F_NATLOOK		0x00001000
#define F_DEMOTE		0x00002000
#define F_LOOKUP_PATH		0x00004000
#define F_DEMOTED		0x00008000
#define F_UDP			0x00010000
#define F_RETURN		0x00020000
#define F_SNMP			0x00040000
#define F_NEEDPF		0x00080000
#define F_PORT			0x00100000
#define F_TLSCLIENT		0x00200000
#define F_NEEDRT		0x00400000
#define F_MATCH			0x00800000
#define F_DIVERT		0x01000000
#define F_SCRIPT		0x02000000
#define F_TLSINSPECT		0x04000000
#define F_HASHKEY		0x08000000

#define F_BITS								\
	"\10\01DISABLE\02BACKUP\03USED\04DOWN\05ADD\06DEL\07CHANGED"	\
	"\10STICKY-ADDRESS\11CHECK_DONE\12ACTIVE_RULESET\13CHECK_SENT"	\
	"\14TLS\15NAT_LOOKUP\16DEMOTE\17LOOKUP_PATH\20DEMOTED\21UDP"	\
	"\22RETURN\23TRAP\24NEEDPF\25PORT\26TLS_CLIENT\27NEEDRT"	\
	"\30MATCH\31DIVERT\32SCRIPT\33TLS_INSPECT\34HASHKEY"

enum forwardmode {
	FWD_NORMAL		= 0,
	FWD_ROUTE,
	FWD_TRANS
};

struct host_config {
	objid_t			 id;
	objid_t			 parentid;
	objid_t			 tableid;
	int			 retry;
	char			 name[MAXHOSTNAMELEN];
	struct sockaddr_storage	 ss;
	int			 ttl;
	int			 priority;
};

struct host {
	TAILQ_ENTRY(host)	 entry;
	TAILQ_ENTRY(host)	 globalentry;
	SLIST_ENTRY(host)	 child;
	SLIST_HEAD(,host)	 children;
	struct host_config	 conf;
	u_int32_t		 flags;
	char			*tablename;
	int			 up;
	int			 last_up;
	u_long			 check_cnt;
	u_long			 up_cnt;
	int			 retry_cnt;
	int			 idx;
	u_int16_t		 he;
	struct ctl_tcp_event	 cte;
};
TAILQ_HEAD(hostlist, host);

enum host_error {
	HCE_NONE		= 0,
	HCE_ABORT,
	HCE_INTERVAL_TIMEOUT,
	HCE_ICMP_OK,
	HCE_ICMP_READ_TIMEOUT,
	HCE_ICMP_WRITE_TIMEOUT,
	HCE_TCP_SOCKET_ERROR,
	HCE_TCP_SOCKET_LIMIT,
	HCE_TCP_SOCKET_OPTION,
	HCE_TCP_CONNECT_FAIL,
	HCE_TCP_CONNECT_TIMEOUT,
	HCE_TCP_CONNECT_OK,
	HCE_TCP_WRITE_TIMEOUT,
	HCE_TCP_WRITE_FAIL,
	HCE_TCP_READ_TIMEOUT,
	HCE_TCP_READ_FAIL,
	HCE_SCRIPT_OK,
	HCE_SCRIPT_FAIL,
	HCE_TLS_CONNECT_ERROR,
	HCE_TLS_CONNECT_FAIL,
	HCE_TLS_CONNECT_OK,
	HCE_TLS_CONNECT_TIMEOUT,
	HCE_TLS_READ_TIMEOUT,
	HCE_TLS_WRITE_TIMEOUT,
	HCE_TLS_READ_ERROR,
	HCE_TLS_WRITE_ERROR,
	HCE_SEND_EXPECT_FAIL,
	HCE_SEND_EXPECT_OK,
	HCE_HTTP_CODE_ERROR,
	HCE_HTTP_CODE_FAIL,
	HCE_HTTP_CODE_OK,
	HCE_HTTP_DIGEST_ERROR,
	HCE_HTTP_DIGEST_FAIL,
	HCE_HTTP_DIGEST_OK,
};

enum host_status {
	HOST_DOWN	= -1,
	HOST_UNKNOWN	= 0,
	HOST_UP		= 1
};
#define HOST_ISUP(x)	(x == HOST_UP)

struct table_config {
	objid_t			 id;
	objid_t			 rdrid;
	u_int32_t		 flags;
	int			 check;
	char			 demote_group[IFNAMSIZ];
	char			 ifname[IFNAMSIZ];
	struct timeval		 timeout;
	in_port_t		 port;
	int			 retcode;
	int			 skip_cnt;
	char			 name[TABLE_NAME_SIZE];
	size_t			 name_len;
	char			 path[MAXPATHLEN];
	char			 exbuf[64];
	char			 digest[41]; /* length of sha1 digest * 2 */
	u_int8_t		 digest_type;
	enum forwardmode	 fwdmode;
};

struct table {
	TAILQ_ENTRY(table)	 entry;
	struct table_config	 conf;
	int			 up;
	int			 skipped;
	struct hostlist		 hosts;
	SSL_CTX			*ssl_ctx;	/* libssl context */
	char			*sendbuf;
};
TAILQ_HEAD(tablelist, table);

enum table_check {
	CHECK_NOCHECK		= 0,
	CHECK_ICMP		= 1,
	CHECK_TCP		= 2,
	CHECK_HTTP_CODE		= 3,
	CHECK_HTTP_DIGEST	= 4,
	CHECK_SEND_EXPECT	= 5,
	CHECK_SCRIPT		= 6
};

struct rdr_config {
	objid_t			 id;
	u_int32_t		 flags;
	in_port_t		 port;
	objid_t			 table_id;
	objid_t			 backup_id;
	int			 mode;
	union hashkey		 key;
	char			 name[SRV_NAME_SIZE];
	char			 tag[RD_TAG_NAME_SIZE];
	struct timeval		 timeout;
};

struct rdr {
	TAILQ_ENTRY(rdr)	 entry;
	struct rdr_config	 conf;
	struct addresslist	 virts;
	struct table		*table;
	struct table		*backup; /* use this if no host up */
	struct ctl_stats	 stats;
};
TAILQ_HEAD(rdrlist, rdr);

struct relay;
struct rsession {
	objid_t				 se_id;
	objid_t				 se_relayid;
	struct ctl_relay_event		 se_in;
	struct ctl_relay_event		 se_out;
	void				*se_priv;
	SIPHASH_CTX			 se_siphashctx;
	struct relay_table		*se_table;
	struct event			 se_ev;
	struct timeval			 se_timeout;
	struct timeval			 se_tv_start;
	struct timeval			 se_tv_last;
	struct event			 se_inflightevt;
	int				 se_done;
	int				 se_retry;
	int				 se_retrycount;
	int				 se_connectcount;
	int				 se_haslog;
	struct evbuffer			*se_log;
	struct relay			*se_relay;
	struct ctl_natlook		*se_cnl;
	int				 se_bnds;
	u_int16_t			 se_tag;
	u_int16_t			 se_label;

	int				 se_cid;
	pid_t				 se_pid;
	SPLAY_ENTRY(rsession)		 se_nodes;
	TAILQ_ENTRY(rsession)		 se_entry;
};
SPLAY_HEAD(session_tree, rsession);
TAILQ_HEAD(sessionlist, rsession);

enum prototype {
	RELAY_PROTO_TCP		= 0,
	RELAY_PROTO_HTTP,
	RELAY_PROTO_DNS
};

enum relay_result {
	RES_DROP		= 0,
	RES_PASS		= 1,
	RES_FAIL		= -1
};

enum rule_action {
	RULE_ACTION_MATCH	= 0,
	RULE_ACTION_PASS,
	RULE_ACTION_BLOCK
};

struct rule_addr {
	int				 addr_af;
	struct sockaddr_storage		 addr;
	u_int8_t			 addr_mask;
	int				 addr_net;
	in_port_t			 addr_port;
};

#define RELAY_ADDR_EQ(_a, _b)						\
	((_a)->addr_mask == (_b)->addr_mask &&				\
	sockaddr_cmp((struct sockaddr *)&(_a)->addr,			\
	(struct sockaddr *)&(_b)->addr, (_a)->addr_mask) == 0)

#define RELAY_ADDR_CMP(_a, _b)						\
	sockaddr_cmp((struct sockaddr *)&(_a)->addr,			\
	(struct sockaddr *)(_b), (_a)->addr_mask)

#define RELAY_ADDR_NEQ(_a, _b)						\
	((_a)->addr_mask != (_b)->addr_mask ||				\
	sockaddr_cmp((struct sockaddr *)&(_a)->addr,			\
	(struct sockaddr *)&(_b)->addr, (_a)->addr_mask) != 0)

struct relay_rule {
	objid_t			 rule_id;
	objid_t			 rule_protoid;

	u_int			 rule_action;
#define RULE_SKIP_PROTO		 0
#define RULE_SKIP_DIR		 1
#define RULE_SKIP_AF		 2
#define RULE_SKIP_SRC		 3
#define RULE_SKIP_DST		 4
#define RULE_SKIP_METHOD	 5
#define RULE_SKIP_COUNT		 6
	struct relay_rule	*rule_skip[RULE_SKIP_COUNT];

#define RULE_FLAG_QUICK		0x01
	u_int8_t		 rule_flags;

	int			 rule_label;
	int			 rule_tag;
	int			 rule_tagged;
	enum direction		 rule_dir;
	u_int			 rule_proto;
	int			 rule_af;
	struct rule_addr	 rule_src;
	struct rule_addr	 rule_dst;
	struct relay_table	*rule_table;

	u_int			 rule_method;
	char			 rule_labelname[LABEL_NAME_SIZE];
	char			 rule_tablename[TABLE_NAME_SIZE];
	char			 rule_taggedname[TAG_NAME_SIZE];
	char			 rule_tagname[TAG_NAME_SIZE];

	struct ctl_rule		 rule_ctl;
	struct kv		 rule_kv[KEY_TYPE_MAX];
	struct kvlist		 rule_kvlist;

	TAILQ_ENTRY(relay_rule)	 rule_entry;
};
TAILQ_HEAD(relay_rules, relay_rule);

#define TCPFLAG_NODELAY		0x01
#define TCPFLAG_NNODELAY	0x02
#define TCPFLAG_SACK		0x04
#define TCPFLAG_NSACK		0x08
#define TCPFLAG_BUFSIZ		0x10
#define TCPFLAG_IPTTL		0x20
#define TCPFLAG_IPMINTTL	0x40
#define TCPFLAG_NSPLICE		0x80
#define TCPFLAG_DEFAULT		0x00

#define TCPFLAG_BITS						\
	"\10\01NODELAY\02NO_NODELAY\03SACK\04NO_SACK"		\
	"\05SOCKET_BUFFER_SIZE\06IP_TTL\07IP_MINTTL\10NO_SPLICE"

#define TLSFLAG_SSLV3				0x01
#define TLSFLAG_TLSV1_0				0x02
#define TLSFLAG_TLSV1_1				0x04
#define TLSFLAG_TLSV1_2				0x08
#define TLSFLAG_TLSV1				0x0e
#define TLSFLAG_VERSION				0x1f
#define TLSFLAG_CIPHER_SERVER_PREF		0x20
#define TLSFLAG_CLIENT_RENEG			0x40
#define TLSFLAG_DEFAULT				\
	(TLSFLAG_TLSV1|TLSFLAG_CLIENT_RENEG)

#define TLSFLAG_BITS						\
	"\06\01sslv3\02tlsv1.0\03tlsv1.1\04tlsv1.2"	\
	"\06cipher-server-preference\07client-renegotiation"

#define TLSCIPHERS_DEFAULT	"HIGH:!aNULL"
#define TLSECDHCURVE_DEFAULT	NID_X9_62_prime256v1

#define TLSDHPARAMS_NONE	0
#define TLSDHPARAMS_DEFAULT	0
#define TLSDHPARAMS_MIN		1024

struct protocol {
	objid_t			 id;
	u_int32_t		 flags;
	u_int8_t		 tcpflags;
	int			 tcpbufsiz;
	int			 tcpbacklog;
	u_int8_t		 tcpipttl;
	u_int8_t		 tcpipminttl;
	u_int8_t		 tlsflags;
	char			 tlsciphers[768];
	int			 tlsdhparams;
	int			 tlsecdhcurve;
	char			 tlsca[MAXPATHLEN];
	char			 tlscacert[MAXPATHLEN];
	char			 tlscakey[MAXPATHLEN];
	char			*tlscapass;
	char			 name[MAX_NAME_SIZE];
	int			 cache;
	enum prototype		 type;
	char			*style;

	int			(*cmp)(struct rsession *, struct rsession *);
	void			*(*validate)(struct rsession *, struct relay *,
				    struct sockaddr_storage *,
				    u_int8_t *, size_t);
	int			(*request)(struct rsession *);
	void			(*close)(struct rsession *);

	struct relay_rules	 rules;
	int			 rulecount;

	TAILQ_ENTRY(protocol)	 entry;
};
TAILQ_HEAD(protolist, protocol);

struct relay_table {
	struct table		*rlt_table;
	u_int32_t		 rlt_flags;
	int			 rlt_mode;
	u_int32_t		 rlt_index;
	struct host		*rlt_host[RELAY_MAXHOSTS];
	int			 rlt_nhosts;
	TAILQ_ENTRY(relay_table) rlt_entry;
};
TAILQ_HEAD(relaytables, relay_table);

struct ca_pkey {
	objid_t			 pkey_id;
	EVP_PKEY		*pkey;
	TAILQ_ENTRY(ca_pkey)	 pkey_entry;
};
TAILQ_HEAD(ca_pkeylist, ca_pkey);

struct relay_config {
	objid_t			 id;
	u_int32_t		 flags;
	objid_t			 proto;
	char			 name[MAXHOSTNAMELEN];
	in_port_t		 port;
	in_port_t		 dstport;
	int			 dstretry;
	struct sockaddr_storage	 ss;
	struct sockaddr_storage	 dstss;
	struct sockaddr_storage	 dstaf;
	struct timeval		 timeout;
	enum forwardmode	 fwdmode;
	union hashkey		 hashkey;
	off_t			 tls_cert_len;
	off_t			 tls_key_len;
	objid_t			 tls_keyid;
	off_t			 tls_ca_len;
	off_t			 tls_cacert_len;
	off_t			 tls_cakey_len;
	objid_t			 tls_cakeyid;
};

struct relay {
	TAILQ_ENTRY(relay)	 rl_entry;
	struct relay_config	 rl_conf;

	int			 rl_up;
	struct protocol		*rl_proto;
	int			 rl_s;
	struct bufferevent	*rl_bev;

	int			 rl_dsts;
	struct bufferevent	*rl_dstbev;

	struct relaytables	 rl_tables;

	struct event		 rl_ev;
	struct event		 rl_evt;

	SSL_CTX			*rl_ssl_ctx;	/* libssl context */

	char			*rl_tls_cert;
	X509			*rl_tls_x509;
	char			*rl_tls_key;
	EVP_PKEY		*rl_tls_pkey;
	char			*rl_tls_ca;
	char			*rl_tls_cacert;
	X509			*rl_tls_cacertx509;
	char			*rl_tls_cakey;
	EVP_PKEY		*rl_tls_capkey;

	struct ctl_stats	 rl_stats[RELAY_MAXPROC + 1];

	struct session_tree	 rl_sessions;
};
TAILQ_HEAD(relaylist, relay);

enum dstmode {
	RELAY_DSTMODE_LOADBALANCE = 0,
	RELAY_DSTMODE_ROUNDROBIN,
	RELAY_DSTMODE_HASH,
	RELAY_DSTMODE_SRCHASH,
	RELAY_DSTMODE_LEASTSTATES,
	RELAY_DSTMODE_RANDOM
};
#define RELAY_DSTMODE_DEFAULT		RELAY_DSTMODE_ROUNDROBIN

struct router;
struct netroute_config {
	objid_t			 id;
	struct sockaddr_storage	 ss;
	int			 prefixlen;
	objid_t			 routerid;
};

struct netroute {
	struct netroute_config	 nr_conf;

	TAILQ_ENTRY(netroute)	 nr_entry;
	TAILQ_ENTRY(netroute)	 nr_route;

	struct router		*nr_router;
};
TAILQ_HEAD(netroutelist, netroute);

struct router_config {
	objid_t			 id;
	u_int32_t		 flags;
	char			 name[MAXHOSTNAMELEN];
	char			 label[RT_LABEL_SIZE];
	int			 nroutes;
	objid_t			 gwtable;
	in_port_t		 gwport;
	int			 rtable;
	int			 af;
};

struct router {
	struct router_config	 rt_conf;

	struct table		*rt_gwtable;
	struct netroutelist	 rt_netroutes;

	TAILQ_ENTRY(router)	 rt_entry;
};
TAILQ_HEAD(routerlist, router);

struct ctl_netroute {
	int			up;
	struct host_config	host;
	struct netroute_config	nr;
	struct router_config	rt;
};

/* initially control.h */
struct control_sock {
	const char	*cs_name;
	struct event	 cs_ev;
	struct event	 cs_evt;
	int		 cs_fd;
	int		 cs_restricted;
	void		*cs_env;

	TAILQ_ENTRY(control_sock) cs_entry;
};
TAILQ_HEAD(control_socks, control_sock);

struct {
	struct event	 ev;
	int		 fd;
} control_state;

enum blockmodes {
	BM_NORMAL,
	BM_NONBLOCK
};


struct imsgev {
	struct imsgbuf		 ibuf;
	void			(*handler)(int, short, void *);
	struct event		 ev;
	struct privsep_proc	*proc;
	void			*data;
	short			 events;
};

#define IMSG_SIZE_CHECK(imsg, p) do {				\
	if (IMSG_DATA_SIZE(imsg) < sizeof(*p))			\
		fatalx("bad length imsg received");		\
} while (0)
#define IMSG_DATA_SIZE(imsg)	((imsg)->hdr.len - IMSG_HEADER_SIZE)

struct ctl_conn {
	TAILQ_ENTRY(ctl_conn)	 entry;
	u_int8_t		 flags;
	u_int			 waiting;
#define CTL_CONN_NOTIFY		 0x01
	struct imsgev		 iev;

};
TAILQ_HEAD(ctl_connlist, ctl_conn);

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_OK,		/* answer to relayctl requests */
	IMSG_CTL_FAIL,
	IMSG_CTL_VERBOSE,
	IMSG_CTL_END,
	IMSG_CTL_RDR,
	IMSG_CTL_TABLE,
	IMSG_CTL_HOST,
	IMSG_CTL_RELAY,
	IMSG_CTL_SESSION,
	IMSG_CTL_ROUTER,
	IMSG_CTL_NETROUTE,
	IMSG_CTL_TABLE_CHANGED,
	IMSG_CTL_PULL_RULESET,
	IMSG_CTL_PUSH_RULESET,
	IMSG_CTL_SHOW_SUM,	/* relayctl requests */
	IMSG_CTL_RDR_ENABLE,
	IMSG_CTL_RDR_DISABLE,
	IMSG_CTL_TABLE_ENABLE,
	IMSG_CTL_TABLE_DISABLE,
	IMSG_CTL_HOST_ENABLE,
	IMSG_CTL_HOST_DISABLE,
	IMSG_CTL_SHUTDOWN,
	IMSG_CTL_START,
	IMSG_CTL_RELOAD,
	IMSG_CTL_RESET,
	IMSG_CTL_POLL,
	IMSG_CTL_NOTIFY,
	IMSG_CTL_RDR_STATS,
	IMSG_CTL_RELAY_STATS,
	IMSG_RDR_ENABLE,	/* notifies from pfe to hce */
	IMSG_RDR_DISABLE,
	IMSG_TABLE_ENABLE,
	IMSG_TABLE_DISABLE,
	IMSG_HOST_ENABLE,
	IMSG_HOST_DISABLE,
	IMSG_HOST_STATUS,	/* notifies from hce to pfe */
	IMSG_SYNC,
	IMSG_NATLOOK,
	IMSG_DEMOTE,
	IMSG_STATISTICS,
	IMSG_SCRIPT,
	IMSG_SNMPSOCK,
	IMSG_BINDANY,
	IMSG_RTMSG,		/* from pfe to parent */
	IMSG_CFG_TABLE,		/* configuration from parent */
	IMSG_CFG_HOST,
	IMSG_CFG_RDR,
	IMSG_CFG_VIRT,
	IMSG_CFG_ROUTER,
	IMSG_CFG_ROUTE,
	IMSG_CFG_PROTO,
	IMSG_CFG_RULE,
	IMSG_CFG_RELAY,
	IMSG_CFG_RELAY_TABLE,
	IMSG_CFG_DONE,
	IMSG_CA_PRIVENC,
	IMSG_CA_PRIVDEC,
	IMSG_SESS_PUBLISH,	/* from relay to hce */
	IMSG_SESS_UNPUBLISH
};

enum privsep_procid {
	PROC_ALL	= -1,
	PROC_PARENT	= 0,
	PROC_HCE,
	PROC_RELAY,
	PROC_PFE,
	PROC_CA,
	PROC_MAX
} privsep_process;

/* Attach the control socket to the following process */
#define PROC_CONTROL	PROC_PFE

struct privsep_pipes {
	int				*pp_pipes[PROC_MAX];
};

struct privsep {
	struct privsep_pipes		*ps_pipes[PROC_MAX];
	struct privsep_pipes		*ps_pp;

	struct imsgev			*ps_ievs[PROC_MAX];
	const char			*ps_title[PROC_MAX];
	pid_t				 ps_pid[PROC_MAX];
	u_int8_t			 ps_what[PROC_MAX];

	u_int				 ps_instances[PROC_MAX];
	u_int				 ps_ninstances;
	u_int				 ps_instance;

	struct control_sock		 ps_csock;
	struct control_socks		 ps_rcsocks;

	/* Event and signal handlers */
	struct event			 ps_evsigint;
	struct event			 ps_evsigterm;
	struct event			 ps_evsigchld;
	struct event			 ps_evsighup;
	struct event			 ps_evsigpipe;
	struct event			 ps_evsigusr1;

	int				 ps_noaction;
	struct passwd			*ps_pw;
	struct relayd			*ps_env;
};

struct privsep_proc {
	const char		*p_title;
	enum privsep_procid	 p_id;
	int			(*p_cb)(int, struct privsep_proc *,
				    struct imsg *);
	pid_t			(*p_init)(struct privsep *,
				    struct privsep_proc *);
	void			(*p_shutdown)(void);
	u_int			 p_instance;
	const char		*p_chroot;
	struct privsep		*p_ps;
	struct relayd		*p_env;
};

struct relayd {
	u_int8_t		 sc_opts;
	u_int32_t		 sc_flags;
	const char		*sc_conffile;
	struct pfdata		*sc_pf;
	int			 sc_rtsock;
	int			 sc_rtseq;
	int			 sc_tablecount;
	int			 sc_rdrcount;
	int			 sc_protocount;
	int			 sc_relaycount;
	int			 sc_routercount;
	int			 sc_routecount;
	struct timeval		 sc_interval;
	struct timeval		 sc_timeout;
	struct table		 sc_empty_table;
	struct protocol		 sc_proto_default;
	struct event		 sc_ev;
	struct tablelist	*sc_tables;
	struct hostlist		 sc_hosts;
	struct rdrlist		*sc_rdrs;
	struct protolist	*sc_protos;
	struct relaylist	*sc_relays;
	struct routerlist	*sc_rts;
	struct netroutelist	*sc_routes;
	struct ca_pkeylist	*sc_pkeys;
	struct sessionlist	 sc_sessions;
	u_int16_t		 sc_prefork_relay;
	char			 sc_demote_group[IFNAMSIZ];
	u_int16_t		 sc_id;

	struct event		 sc_statev;
	struct timeval		 sc_statinterval;

	int			 sc_snmp;
	const char		*sc_snmp_path;
	int			 sc_snmp_flags;
	struct event		 sc_snmpto;
	struct event		 sc_snmpev;

	int			 sc_has_icmp;
	int			 sc_has_icmp6;
	struct ctl_icmp_event	 sc_icmp_send;
	struct ctl_icmp_event	 sc_icmp_recv;
	struct ctl_icmp_event	 sc_icmp6_send;
	struct ctl_icmp_event	 sc_icmp6_recv;

	struct privsep		*sc_ps;
	int			 sc_reload;
};

#define	FSNMP_TRAPONLY			0x01

#define RELAYD_OPT_VERBOSE		0x01
#define RELAYD_OPT_NOACTION		0x04
#define RELAYD_OPT_LOGUPDATE		0x08
#define RELAYD_OPT_LOGNOTIFY		0x10
#define RELAYD_OPT_LOGALL		0x18

/* control.c */
int	 control_init(struct privsep *, struct control_sock *);
int	 control_listen(struct control_sock *);
void	 control_cleanup(struct control_sock *);
void	 control_dispatch_imsg(int, short, void *);
void	 control_imsg_forward(struct imsg *);
struct ctl_conn	*
	 control_connbyfd(int);
void	 socket_set_blockmode(int, enum blockmodes);

extern  struct ctl_connlist ctl_conns;

/* parse.y */
int	 parse_config(const char *, struct relayd *);
int	 load_config(const char *, struct relayd *);
int	 cmdline_symset(char *);

/* log.c */
const char *host_error(enum host_error);
const char *host_status(enum host_status);
const char *table_check(enum table_check);
const char *print_availability(u_long, u_long);
const char *print_host(struct sockaddr_storage *, char *, size_t);
const char *print_time(struct timeval *, struct timeval *, char *, size_t);
const char *printb_flags(const u_int32_t, const char *);
void	 getmonotime(struct timeval *);

/* pfe.c */
pid_t	 pfe(struct privsep *, struct privsep_proc *);
void	 show(struct ctl_conn *);
void	 show_sessions(struct ctl_conn *);
int	 enable_rdr(struct ctl_conn *, struct ctl_id *);
int	 enable_table(struct ctl_conn *, struct ctl_id *);
int	 enable_host(struct ctl_conn *, struct ctl_id *, struct host *);
int	 disable_rdr(struct ctl_conn *, struct ctl_id *);
int	 disable_table(struct ctl_conn *, struct ctl_id *);
int	 disable_host(struct ctl_conn *, struct ctl_id *, struct host *);

/* pfe_filter.c */
void	 init_filter(struct relayd *, int);
void	 init_tables(struct relayd *);
void	 flush_table(struct relayd *, struct rdr *);
void	 sync_table(struct relayd *, struct rdr *, struct table *);
void	 sync_ruleset(struct relayd *, struct rdr *, int);
void	 flush_rulesets(struct relayd *);
int	 natlook(struct relayd *, struct ctl_natlook *);
u_int64_t
	 check_table(struct relayd *, struct rdr *, struct table *);

/* pfe_route.c */
void	 init_routes(struct relayd *);
void	 sync_routes(struct relayd *, struct router *);
int	 pfe_route(struct relayd *, struct ctl_netroute *);

/* hce.c */
pid_t	 hce(struct privsep *, struct privsep_proc *);
void	 hce_notify_done(struct host *, enum host_error);

/* relay.c */
pid_t	 relay(struct privsep *, struct privsep_proc *);
int	 relay_privinit(struct relay *);
void	 relay_notify_done(struct host *, const char *);
int	 relay_session_cmp(struct rsession *, struct rsession *);
int	 relay_load_certfiles(struct relay *);
void	 relay_close(struct rsession *, const char *);
void	 relay_natlook(int, short, void *);
void	 relay_session(struct rsession *);
int	 relay_from_table(struct rsession *);
int	 relay_socket_af(struct sockaddr_storage *, in_port_t);
in_port_t
	 relay_socket_getport(struct sockaddr_storage *);
int	 relay_cmp_af(struct sockaddr_storage *,
	    struct sockaddr_storage *);
void	 relay_write(struct bufferevent *, void *);
void	 relay_read(struct bufferevent *, void *);
int	 relay_splice(struct ctl_relay_event *);
int	 relay_splicelen(struct ctl_relay_event *);
int	 relay_spliceadjust(struct ctl_relay_event *);
void	 relay_error(struct bufferevent *, short, void *);
int	 relay_preconnect(struct rsession *);
int	 relay_connect(struct rsession *);
void	 relay_connected(int, short, void *);
void	 relay_bindanyreq(struct rsession *, in_port_t, int);
void	 relay_bindany(int, short, void *);
void	 relay_dump(struct ctl_relay_event *, const void *, size_t);
int	 relay_bufferevent_add(struct event *, int);
int	 relay_bufferevent_print(struct ctl_relay_event *, const char *);
int	 relay_bufferevent_write_buffer(struct ctl_relay_event *,
	    struct evbuffer *);
int	 relay_bufferevent_write_chunk(struct ctl_relay_event *,
	    struct evbuffer *, size_t);
int	 relay_bufferevent_write(struct ctl_relay_event *,
	    void *, size_t);
int	 relay_test(struct protocol *, struct ctl_relay_event *);
void	 relay_calc_skip_steps(struct relay_rules *);
void	 relay_match(struct kvlist *, struct kv *, struct kv *,
	    struct kvtree *);
void	 relay_session_insert(struct rsession *);
void	 relay_session_remove(struct rsession *);
void	 relay_session_publish(struct rsession *);
void	 relay_session_unpublish(struct rsession *);

SPLAY_PROTOTYPE(session_tree, rsession, se_nodes, relay_session_cmp);

/* relay_http.c */
void	 relay_http(struct relayd *);
void	 relay_http_init(struct relay *);
void	 relay_abort_http(struct rsession *, u_int, const char *,
	    u_int16_t);
void	 relay_read_http(struct bufferevent *, void *);
void	 relay_close_http(struct rsession *);
u_int	 relay_httpmethod_byname(const char *);
const char
	*relay_httpmethod_byid(u_int);
const char
	*relay_httperror_byid(u_int);
int	 relay_httpdesc_init(struct ctl_relay_event *);

/* relay_udp.c */
void	 relay_udp_privinit(struct relayd *, struct relay *);
void	 relay_udp_init(struct relay *);
int	 relay_udp_bind(struct sockaddr_storage *, in_port_t,
	    struct protocol *);
void	 relay_udp_server(int, short, void *);

/* check_icmp.c */
void	 icmp_init(struct relayd *);
void	 schedule_icmp(struct relayd *, struct host *);
void	 check_icmp(struct relayd *, struct timeval *);

/* check_tcp.c */
void	 check_tcp(struct ctl_tcp_event *);

/* check_script.c */
void	 check_script(struct relayd *, struct host *);
void	 script_done(struct relayd *, struct ctl_script *);
int	 script_exec(struct relayd *, struct ctl_script *);

/* ssl.c */
void	 ssl_init(struct relayd *);
void	 ssl_transaction(struct ctl_tcp_event *);
SSL_CTX	*ssl_ctx_create(struct relayd *);
void	 ssl_error(const char *, const char *);
char	*ssl_load_key(struct relayd *, const char *, off_t *, char *);
X509	*ssl_update_certificate(X509 *, EVP_PKEY *, EVP_PKEY *, X509 *);
int	 ssl_load_pkey(const void *, size_t, char *, off_t,
	    X509 **, EVP_PKEY **);
int	 ssl_ctx_fake_private_key(SSL_CTX *, const void *, size_t,
	    char *, off_t, X509 **, EVP_PKEY **);

/* ssl_privsep.c */
int	 ssl_ctx_use_certificate_chain(SSL_CTX *, char *, off_t);
int	 ssl_ctx_load_verify_memory(SSL_CTX *, char *, off_t);

/* ca.c */
pid_t	 ca(struct privsep *, struct privsep_proc *);
void	 ca_engine_init(struct relayd *);

/* relayd.c */
struct host	*host_find(struct relayd *, objid_t);
struct table	*table_find(struct relayd *, objid_t);
struct rdr	*rdr_find(struct relayd *, objid_t);
struct netroute	*route_find(struct relayd *, objid_t);
struct router	*router_find(struct relayd *, objid_t);
struct host	*host_findbyname(struct relayd *, const char *);
struct table	*table_findbyname(struct relayd *, const char *);
struct table	*table_findbyconf(struct relayd *, struct table *);
struct rdr	*rdr_findbyname(struct relayd *, const char *);
void		 event_again(struct event *, int, short,
		    void (*)(int, short, void *),
		    struct timeval *, struct timeval *, void *);
struct relay	*relay_find(struct relayd *, objid_t);
struct protocol	*proto_find(struct relayd *, objid_t);
struct rsession	*session_find(struct relayd *, objid_t);
struct relay	*relay_findbyname(struct relayd *, const char *);
struct relay	*relay_findbyaddr(struct relayd *, struct relay_config *);
EVP_PKEY	*pkey_find(struct relayd *, objid_t);
struct ca_pkey	*pkey_add(struct relayd *, EVP_PKEY *, objid_t);
int		 expand_string(char *, size_t, const char *, const char *);
void		 translate_string(char *);
void		 purge_key(char **, off_t);
void		 purge_table(struct tablelist *, struct table *);
void		 purge_relay(struct relayd *, struct relay *);
char		*digeststr(enum digest_type, const u_int8_t *, size_t, char *);
const char	*canonicalize_host(const char *, char *, size_t);
int		 map6to4(struct sockaddr_storage *);
int		 map4to6(struct sockaddr_storage *, struct sockaddr_storage *);
void		 imsg_event_add(struct imsgev *);
int		 imsg_compose_event(struct imsgev *, u_int16_t, u_int32_t,
		    pid_t, int, void *, u_int16_t);
void		 socket_rlimit(int);
char		*get_string(u_int8_t *, size_t);
void		*get_data(u_int8_t *, size_t);
int		 sockaddr_cmp(struct sockaddr *, struct sockaddr *, int);
struct in6_addr *prefixlen2mask6(u_int8_t, u_int32_t *);
u_int32_t	 prefixlen2mask(u_int8_t);
int		 accept_reserve(int, struct sockaddr *, socklen_t *, int,
		     volatile int *);
struct kv	*kv_add(struct kvtree *, char *, char *);
int		 kv_set(struct kv *, char *, ...);
int		 kv_setkey(struct kv *, char *, ...);
void		 kv_delete(struct kvtree *, struct kv *);
struct kv	*kv_extend(struct kvtree *, struct kv *, char *);
void		 kv_purge(struct kvtree *);
void		 kv_free(struct kv *);
struct kv	*kv_inherit(struct kv *, struct kv *);
void		 relay_log(struct rsession *, char *);
int		 kv_log(struct rsession *, struct kv *, u_int16_t,
		     enum direction);
struct kv	*kv_find(struct kvtree *, struct kv *);
int		 kv_cmp(struct kv *, struct kv *);
int		 rule_add(struct protocol *, struct relay_rule *, const char
		     *);
void		 rule_delete(struct relay_rules *, struct relay_rule *);
void		 rule_free(struct relay_rule *);
struct relay_rule
		*rule_inherit(struct relay_rule *);
void		 rule_settable(struct relay_rules *, struct relay_table *);
RB_PROTOTYPE(kvtree, kv, kv_node, kv_cmp);

/* carp.c */
int	 carp_demote_init(char *, int);
void	 carp_demote_shutdown(void);
int	 carp_demote_get(char *);
int	 carp_demote_set(char *, int);
int	 carp_demote_reset(char *, int);

/* name2id.c */
u_int16_t	 label_name2id(const char *);
const char	*label_id2name(u_int16_t);
void		 label_unref(u_int16_t);
void		 label_ref(u_int16_t);
u_int16_t	 tag_name2id(const char *);
const char	*tag_id2name(u_int16_t);
void		 tag_unref(u_int16_t);
void		 tag_ref(u_int16_t);

/* snmp.c */
void	 snmp_init(struct relayd *, enum privsep_procid);
void	 snmp_setsock(struct relayd *, enum privsep_procid);
int	 snmp_getsock(struct relayd *, struct imsg *);
void	 snmp_hosttrap(struct relayd *, struct table *, struct host *);

/* shuffle.c */
void		shuffle_init(struct shuffle *);
u_int16_t	shuffle_generate16(struct shuffle *);

/* log.c */
void	log_init(int);
void	log_verbose(int);
void	log_warn(const char *, ...) __attribute__((__format__ (printf, 1, 2)));
void	log_warnx(const char *, ...) __attribute__((__format__ (printf, 1, 2)));
void	log_info(const char *, ...) __attribute__((__format__ (printf, 1, 2)));
void	log_debug(const char *, ...) __attribute__((__format__ (printf, 1, 2)));
void	logit(int, const char *, ...) __attribute__((__format__ (printf, 2, 3)));
void	vlog(int, const char *, va_list) __attribute__((__format__ (printf, 2, 0)));
__dead void fatal(const char *);
__dead void fatalx(const char *);

/* proc.c */
void	 proc_init(struct privsep *, struct privsep_proc *, u_int);
void	 proc_kill(struct privsep *);
void	 proc_listen(struct privsep *, struct privsep_proc *, size_t);
void	 proc_dispatch(int, short event, void *);
pid_t	 proc_run(struct privsep *, struct privsep_proc *,
	    struct privsep_proc *, u_int,
	    void (*)(struct privsep *, struct privsep_proc *, void *), void *);
void	 proc_range(struct privsep *, enum privsep_procid, int *, int *);
int	 proc_compose_imsg(struct privsep *, enum privsep_procid, int,
	    u_int16_t, int, void *, u_int16_t);
int	 proc_composev_imsg(struct privsep *, enum privsep_procid, int,
	    u_int16_t, int, const struct iovec *, int);
int	 proc_forward_imsg(struct privsep *, struct imsg *,
	    enum privsep_procid, int);
struct imsgbuf *
	 proc_ibuf(struct privsep *, enum privsep_procid, int);
struct imsgev *
	 proc_iev(struct privsep *, enum privsep_procid, int);
void	 imsg_event_add(struct imsgev *);
int	 imsg_compose_event(struct imsgev *, u_int16_t, u_int32_t,
	    pid_t, int, void *, u_int16_t);
int	 imsg_composev_event(struct imsgev *, u_int16_t, u_int32_t,
	    pid_t, int, const struct iovec *, int);

/* config.c */
int	 config_init(struct relayd *);
void	 config_purge(struct relayd *, u_int);
int	 config_setreset(struct relayd *, u_int);
int	 config_getreset(struct relayd *, struct imsg *);
int	 config_getcfg(struct relayd *, struct imsg *);
int	 config_settable(struct relayd *, struct table *);
int	 config_gettable(struct relayd *, struct imsg *);
int	 config_gethost(struct relayd *, struct imsg *);
int	 config_setrdr(struct relayd *, struct rdr *);
int	 config_getrdr(struct relayd *, struct imsg *);
int	 config_getvirt(struct relayd *, struct imsg *);
int	 config_setrt(struct relayd *, struct router *);
int	 config_getrt(struct relayd *, struct imsg *);
int	 config_getroute(struct relayd *, struct imsg *);
int	 config_setproto(struct relayd *, struct protocol *);
int	 config_getproto(struct relayd *, struct imsg *);
int	 config_setrule(struct relayd *, struct protocol *);
int	 config_getrule(struct relayd *, struct imsg *);
int	 config_setrelay(struct relayd *, struct relay *);
int	 config_getrelay(struct relayd *, struct imsg *);
int	 config_getrelaytable(struct relayd *, struct imsg *);

#endif /* _RELAYD_H */
