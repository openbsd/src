/*	$OpenBSD: relayd.h,v 1.133 2010/01/11 06:40:14 jsg Exp $	*/

/*
 * Copyright (c) 2006, 2007 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2006, 2007, 2008 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/tree.h>

#include <imsg.h>

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
#define TABLE_NAME_SIZE		64
#define	TAG_NAME_SIZE		64
#define	RT_LABEL_SIZE		32
#define SRV_NAME_SIZE		64
#define MAX_NAME_SIZE		64
#define SRV_MAX_VIRTS		16

#define RELAY_MAX_SESSIONS	1024
#define RELAY_TIMEOUT		600
#define RELAY_CACHESIZE		-1	/* use default size */
#define RELAY_NUMPROC		5
#define RELAY_MAXPROC		32
#define RELAY_MAXHOSTS		32
#define RELAY_STATINTERVAL	60
#define RELAY_BACKLOG		10
#define RELAY_MAXLOOKUPLEVELS	5

#define SMALL_READ_BUF_SIZE	1024
#define ICMP_BUF_SIZE		64

#define PURGE_TABLES		0x01
#define PURGE_RDRS		0x02
#define PURGE_RELAYS		0x04
#define PURGE_PROTOS		0x08
#define PURGE_EVERYTHING	0xff

#define SNMP_RECONNECT_TIMEOUT	{ 3, 0 }	/* sec, usec */

#if DEBUG > 1
#define DPRINTF		log_debug
#else
#define DPRINTF(x...)	do {} while(0)
#endif

/* Used for DNS request ID randomization */
struct shuffle {
	u_int16_t	 id_shuffle[65536];
	int		 isindex;
};


typedef u_int32_t objid_t;

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

struct ctl_script {
	objid_t		 host;
	int		 retval;
};

struct ctl_demote {
	char		 group[IFNAMSIZ];
	int		 level;
};

struct ctl_netroute {
	objid_t		 id;
	objid_t		 hostid;
	int		 up;
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
	struct buf		*buf;
	struct host		*host;
	struct table		*table;
	struct timeval		 tv_start;
	struct event		 ev;
	int			(*validate_read)(struct ctl_tcp_event *);
	int			(*validate_close)(struct ctl_tcp_event *);
	SSL			*ssl;
};

enum httpmethod {
	HTTP_METHOD_NONE	= 0,
	HTTP_METHOD_GET		= 1,
	HTTP_METHOD_HEAD	= 2,
	HTTP_METHOD_POST	= 3,
	HTTP_METHOD_PUT		= 4,
	HTTP_METHOD_DELETE	= 5,
	HTTP_METHOD_OPTIONS	= 6,
	HTTP_METHOD_TRACE	= 7,
	HTTP_METHOD_CONNECT	= 8,
	HTTP_METHOD_RESPONSE	= 9	/* Server response */
};

enum direction {
	RELAY_DIR_REQUEST	= 0,
	RELAY_DIR_RESPONSE	= 1
};

struct ctl_relay_event {
	int			 s;
	in_port_t		 port;
	struct sockaddr_storage	 ss;
	struct bufferevent	*bev;
	struct evbuffer		*output;
	struct ctl_relay_event	*dst;
	void			*con;
	SSL			*ssl;
	u_int8_t		*nodes;
	struct proto_tree	*tree;

	char			*path;
	char			*args;
	char			*version;

	int			 line;
	size_t			 toread;
	int			 chunked;
	int			 done;
	enum httpmethod		 method;
	enum direction		 dir;

	u_int8_t		*buf;
	int			 buflen;
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

struct ctl_stats {
	objid_t			 id;
	int			 proc;

	u_int			 interval;
	u_int64_t		 cnt;
	u_int32_t		 tick;
	u_int32_t		 avg;
	u_int32_t		 last;
	u_int32_t		 avg_hour;
	u_int32_t		 last_hour;
	u_int32_t		 avg_day;
	u_int32_t		 last_day;
};

struct portrange {
	in_port_t		 val[2];
	u_int8_t		 op;
};

struct address {
	struct sockaddr_storage	 ss;
	int			 ipproto;
	struct portrange	 port;
	char			 ifname[IFNAMSIZ];
	TAILQ_ENTRY(address)	 entry;
};
TAILQ_HEAD(addresslist, address);

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
#define F_SSL			0x00000800
#define F_NATLOOK		0x00001000
#define F_DEMOTE		0x00002000
#define F_LOOKUP_PATH		0x00004000
#define F_DEMOTED		0x00008000
#define F_UDP			0x00010000
#define F_RETURN		0x00020000
#define F_TRAP			0x00040000
#define F_NEEDPF		0x00080000
#define F_PORT			0x00100000
#define F_SSLCLIENT		0x00200000
#define F_NEEDRT		0x00400000

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
};

struct host {
	TAILQ_ENTRY(host)	 entry;
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
	HCE_TCP_CONNECT_ERROR,
	HCE_TCP_CONNECT_FAIL,
	HCE_TCP_CONNECT_TIMEOUT,
	HCE_TCP_CONNECT_OK,
	HCE_TCP_WRITE_TIMEOUT,
	HCE_TCP_WRITE_FAIL,
	HCE_TCP_READ_TIMEOUT,
	HCE_TCP_READ_FAIL,
	HCE_SCRIPT_OK,
	HCE_SCRIPT_FAIL,
	HCE_SSL_CONNECT_ERROR,
	HCE_SSL_CONNECT_FAIL,
	HCE_SSL_CONNECT_OK,
	HCE_SSL_CONNECT_TIMEOUT,
	HCE_SSL_READ_TIMEOUT,
	HCE_SSL_WRITE_TIMEOUT,
	HCE_SSL_READ_ERROR,
	HCE_SSL_WRITE_ERROR,
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

enum digest_type {
	DIGEST_NONE		= 0,
	DIGEST_SHA1		= 1,
	DIGEST_MD5		= 2
};

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
	SSL_CTX			*ssl_ctx;
	int			 sendbuf_len;
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
	char			 name[SRV_NAME_SIZE];
	char			 tag[TAG_NAME_SIZE];
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
	u_int32_t			 se_hashkey;
	struct event			 se_ev;
	struct timeval			 se_timeout;
	struct timeval			 se_tv_start;
	struct timeval			 se_tv_last;
	int				 se_done;
	int				 se_retry;
	u_int16_t			 se_mark;
	struct evbuffer			*se_log;
	struct relay			*se_relay;
	struct ctl_natlook		*se_cnl;
	int				 se_bnds;

	SPLAY_ENTRY(rsession)		 se_nodes;
};
SPLAY_HEAD(session_tree, rsession);

enum nodeaction {
	NODE_ACTION_NONE	= 0,
	NODE_ACTION_APPEND	= 1,
	NODE_ACTION_CHANGE	= 2,
	NODE_ACTION_REMOVE	= 3,
	NODE_ACTION_EXPECT	= 4,
	NODE_ACTION_FILTER	= 5,
	NODE_ACTION_HASH	= 6,
	NODE_ACTION_LOG		= 7,
	NODE_ACTION_MARK	= 8
};

enum nodetype {
	NODE_TYPE_HEADER	= 0,
	NODE_TYPE_QUERY		= 1,
	NODE_TYPE_COOKIE	= 2,
	NODE_TYPE_PATH		= 3,
	NODE_TYPE_URL		= 4
};

#define PNFLAG_MACRO			0x01
#define PNFLAG_MARK			0x02
#define PNFLAG_LOG			0x04
#define PNFLAG_LOOKUP_QUERY		0x08
#define PNFLAG_LOOKUP_COOKIE		0x10
#define PNFLAG_LOOKUP_URL		0xe0
#define PNFLAG_LOOKUP_URL_DIGEST	0xc0
#define PNFLAG_LOOKUP_DIGEST(x)		(0x20 << x)

enum noderesult {
	PN_DROP			= 0,
	PN_PASS			= 1,
	PN_FAIL			= -1
};

struct protonode {
	objid_t				 id;
	char				*key;
	enum nodeaction			 action;
	char				*value;
	u_int8_t			 flags;
	enum nodetype			 type;
	u_int16_t			 mark;
	u_int16_t			 label;

	SIMPLEQ_HEAD(, protonode)	 head;
	SIMPLEQ_ENTRY(protonode)	 entry;

	RB_ENTRY(protonode)		 nodes;
};
RB_HEAD(proto_tree, protonode);

#define PROTONODE_FOREACH(elm, root, field)				\
	for (elm = root; elm != NULL; elm = SIMPLEQ_NEXT(elm, entry))	\

enum prototype {
	RELAY_PROTO_TCP		= 0,
	RELAY_PROTO_HTTP	= 1,
	RELAY_PROTO_DNS		= 2
};

#define TCPFLAG_NODELAY		0x01
#define TCPFLAG_NNODELAY	0x02
#define TCPFLAG_SACK		0x04
#define TCPFLAG_NSACK		0x08
#define TCPFLAG_BUFSIZ		0x10
#define TCPFLAG_IPTTL		0x20
#define TCPFLAG_IPMINTTL	0x40
#define TCPFLAG_DEFAULT		0x00

#define SSLFLAG_SSLV2		0x01
#define SSLFLAG_SSLV3		0x02
#define SSLFLAG_TLSV1		0x04
#define SSLFLAG_VERSION		0x07
#define SSLFLAG_DEFAULT		(SSLFLAG_SSLV3|SSLFLAG_TLSV1)

#define SSLCIPHERS_DEFAULT	"HIGH:!ADH"

struct protocol {
	objid_t			 id;
	u_int32_t		 flags;
	u_int8_t		 tcpflags;
	int			 tcpbufsiz;
	int			 tcpbacklog;
	u_int8_t		 tcpipttl;
	u_int8_t		 tcpipminttl;
	u_int8_t		 sslflags;
	char			 sslciphers[768];
	char			*sslca;
	char			 name[MAX_NAME_SIZE];
	int			 cache;
	enum prototype		 type;
	int			 lateconnect;
	char			*style;

	int			 request_nodes;
	struct proto_tree	 request_tree;
	int			 response_nodes;
	struct proto_tree	 response_tree;

	int			(*cmp)(struct rsession *, struct rsession *);
	void			*(*validate)(struct rsession *, struct relay *,
				    struct sockaddr_storage *,
				    u_int8_t *, size_t);
	int			(*request)(struct rsession *);

	TAILQ_ENTRY(protocol)	 entry;
};
TAILQ_HEAD(protolist, protocol);

struct relay_config {
	objid_t			 id;
	u_int32_t		 flags;
	objid_t			 proto;
	char			 name[MAXHOSTNAMELEN];
	char			 ifname[IFNAMSIZ];
	in_port_t		 port;
	in_port_t		 dstport;
	int			 dstmode;
	int			 dstretry;
	objid_t			 dsttable;
	struct sockaddr_storage	 ss;
	struct sockaddr_storage	 dstss;
	struct sockaddr_storage	 dstaf;
	struct timeval		 timeout;
	enum forwardmode	 fwdmode;
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

	struct table		*rl_dsttable;
	u_int32_t		 rl_dstkey;
	struct host		*rl_dsthost[RELAY_MAXHOSTS];
	int			 rl_dstnhosts;

	struct event		 rl_ev;

	SSL_CTX			*rl_ssl_ctx;
	char			*rl_ssl_cert;
	off_t			 rl_ssl_cert_len;
	char			*rl_ssl_key;
	off_t			 rl_ssl_key_len;
	char			*rl_ssl_ca;
	off_t			 rl_ssl_ca_len;

	struct ctl_stats	 rl_stats[RELAY_MAXPROC + 1];

	struct session_tree	 rl_sessions;
};
TAILQ_HEAD(relaylist, relay);

enum dstmode {
	RELAY_DSTMODE_LOADBALANCE	= 0,
	RELAY_DSTMODE_ROUNDROBIN	= 1,
	RELAY_DSTMODE_HASH		= 2
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
};

struct router {
	struct router_config	 rt_conf;
	int			 rt_af;

	struct table		*rt_gwtable;
	struct netroutelist	 rt_netroutes;

	TAILQ_ENTRY(router)	 rt_entry;
};
TAILQ_HEAD(routerlist, router);

enum {
	PROC_MAIN,
	PROC_PFE,
	PROC_HCE,
	PROC_RELAY
} relayd_process;

struct relayd {
	u_int8_t		 sc_opts;
	u_int32_t		 sc_flags;
	const char		*sc_confpath;
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
	struct rdrlist		*sc_rdrs;
	struct protolist	*sc_protos;
	struct relaylist	*sc_relays;
	struct routerlist	*sc_rts;
	struct netroutelist	*sc_routes;
	u_int16_t		 sc_prefork_relay;
	char			 sc_demote_group[IFNAMSIZ];
	u_int16_t		 sc_id;

	struct event		 sc_statev;
	struct timeval		 sc_statinterval;

	int			 sc_snmp;
	struct event		 sc_snmpto;
	struct event		 sc_snmpev;

	int			 sc_has_icmp;
	int			 sc_has_icmp6;
	struct ctl_icmp_event	 sc_icmp_send;
	struct ctl_icmp_event	 sc_icmp_recv;
	struct ctl_icmp_event	 sc_icmp6_send;
	struct ctl_icmp_event	 sc_icmp6_recv;
};

#define RELAYD_OPT_VERBOSE		0x01
#define RELAYD_OPT_NOACTION		0x04
#define RELAYD_OPT_LOGUPDATE		0x08
#define RELAYD_OPT_LOGNOTIFY		0x10
#define RELAYD_OPT_LOGALL		0x18

/* initially control.h */
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
	void			*data;
	short			 events;
};

struct ctl_conn {
	TAILQ_ENTRY(ctl_conn)	 entry;
	u_int8_t		 flags;
#define CTL_CONN_NOTIFY		 0x01
	struct imsgev	 	 iev;

};
TAILQ_HEAD(ctl_connlist, ctl_conn);

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_OK,		/* answer to relayctl requests */
	IMSG_CTL_FAIL,
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
	IMSG_CTL_RELOAD,
	IMSG_CTL_POLL,
	IMSG_CTL_NOTIFY,
	IMSG_CTL_RDR_STATS,
	IMSG_CTL_RELAY_STATS,
	IMSG_CTL_LOG_VERBOSE,
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
	IMSG_RECONF,		/* reconfiguration notifies */
	IMSG_RECONF_TABLE,
	IMSG_RECONF_SENDBUF,
	IMSG_RECONF_HOST,
	IMSG_RECONF_RDR,
	IMSG_RECONF_VIRT,
	IMSG_RECONF_PROTO,
	IMSG_RECONF_REQUEST_TREE,
	IMSG_RECONF_RESPONSE_TREE,
	IMSG_RECONF_PNODE_KEY,
	IMSG_RECONF_PNODE_VAL,
	IMSG_RECONF_RELAY,
	IMSG_RECONF_END,
	IMSG_SCRIPT,
	IMSG_SNMPSOCK,
	IMSG_BINDANY,
	IMSG_RTMSG		/* from pfe to parent */
};

/* control.c */
int	control_init(void);
int	control_listen(struct relayd *, struct imsgev *, struct imsgev *);
void    control_accept(int, short, void *);
void    control_dispatch_imsg(int, short, void *);
void	control_imsg_forward(struct imsg *);
void    control_cleanup(void);

void    session_socket_blockmode(int, enum blockmodes);

extern  struct ctl_connlist ctl_conns;

/* parse.y */
struct relayd	*parse_config(const char *, int);
int		 cmdline_symset(char *);

/* log.c */
const char *host_error(enum host_error);
const char *host_status(enum host_status);
const char *table_check(enum table_check);
const char *print_availability(u_long, u_long);
const char *print_host(struct sockaddr_storage *, char *, size_t);
const char *print_time(struct timeval *, struct timeval *, char *, size_t);
const char *print_httperror(u_int);

/* pfe.c */
pid_t	 pfe(struct relayd *, int [2], int [2], int [RELAY_MAXPROC][2],
	    int [2], int [RELAY_MAXPROC][2]);
void	 show(struct ctl_conn *);
void	 show_sessions(struct ctl_conn *);
int	 enable_rdr(struct ctl_conn *, struct ctl_id *);
int	 enable_table(struct ctl_conn *, struct ctl_id *);
int	 enable_host(struct ctl_conn *, struct ctl_id *, struct host *);
int	 disable_rdr(struct ctl_conn *, struct ctl_id *);
int	 disable_table(struct ctl_conn *, struct ctl_id *);
int	 disable_host(struct ctl_conn *, struct ctl_id *, struct host *);

/* pfe_filter.c */
void	 init_filter(struct relayd *);
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
pid_t	 hce(struct relayd *, int [2], int [2], int [RELAY_MAXPROC][2],
	    int [2], int [RELAY_MAXPROC][2]);
void	 hce_notify_done(struct host *, enum host_error);

/* relay.c */
pid_t	 relay(struct relayd *, int [2], int [2], int [RELAY_MAXPROC][2],
	    int [2], int [RELAY_MAXPROC][2]);
void	 relay_notify_done(struct host *, const char *);
int	 relay_session_cmp(struct rsession *, struct rsession *);
int	 relay_load_certfiles(struct relay *);
void	 relay_close(struct rsession *, const char *);
void	 relay_natlook(int, short, void *);
void	 relay_session(struct rsession *);
int	 relay_from_table(struct rsession *);
int	 relay_socket_af(struct sockaddr_storage *, in_port_t);
int	 relay_cmp_af(struct sockaddr_storage *,
		 struct sockaddr_storage *);


RB_PROTOTYPE(proto_tree, protonode, se_nodes, relay_proto_cmp);
SPLAY_PROTOTYPE(session_tree, rsession, se_nodes, relay_session_cmp);

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
void	 check_script(struct host *);
void	 script_done(struct relayd *, struct ctl_script *);
int	 script_exec(struct relayd *, struct ctl_script *);

/* ssl.c */
void	 ssl_init(struct relayd *);
void	 ssl_transaction(struct ctl_tcp_event *);
SSL_CTX	*ssl_ctx_create(struct relayd *);
void	 ssl_error(const char *, const char *);

/* ssl_privsep.c */
int	 ssl_ctx_use_private_key(SSL_CTX *, char *, off_t);
int	 ssl_ctx_use_certificate_chain(SSL_CTX *, char *, off_t);
int	 ssl_ctx_load_verify_memory(SSL_CTX *, char *, off_t);

/* relayd.c */
struct host	*host_find(struct relayd *, objid_t);
struct table	*table_find(struct relayd *, objid_t);
struct rdr	*rdr_find(struct relayd *, objid_t);
struct netroute	*route_find(struct relayd *, objid_t);
struct host	*host_findbyname(struct relayd *, const char *);
struct table	*table_findbyname(struct relayd *, const char *);
struct table	*table_findbyconf(struct relayd *, struct table *);
struct rdr	*rdr_findbyname(struct relayd *, const char *);
void		 event_again(struct event *, int, short,
		    void (*)(int, short, void *),
		    struct timeval *, struct timeval *, void *);
struct relay	*relay_find(struct relayd *, objid_t);
struct rsession	*session_find(struct relayd *, objid_t);
struct relay	*relay_findbyname(struct relayd *, const char *);
struct relay	*relay_findbyaddr(struct relayd *, struct relay_config *);
int		 expand_string(char *, size_t, const char *, const char *);
void		 translate_string(char *);
void		 purge_config(struct relayd *, u_int8_t);
void		 purge_table(struct tablelist *, struct table *);
void		 merge_config(struct relayd *, struct relayd *);
char		*digeststr(enum digest_type, const u_int8_t *, size_t, char *);
const char	*canonicalize_host(const char *, char *, size_t);
struct protonode *protonode_header(enum direction, struct protocol *,
		    struct protonode *);
int		 protonode_add(enum direction, struct protocol *,
		    struct protonode *);
int		 protonode_load(enum direction, struct protocol *,
		    struct protonode *, const char *);
int		 map6to4(struct sockaddr_storage *);
int		 map4to6(struct sockaddr_storage *, struct sockaddr_storage *);
void		 imsg_event_add(struct imsgev *);
int	 	 imsg_compose_event(struct imsgev *, u_int16_t, u_int32_t,
		    pid_t, int, void *, u_int16_t);

/* carp.c */
int	 carp_demote_init(char *, int);
void	 carp_demote_shutdown(void);
int	 carp_demote_get(char *);
int	 carp_demote_set(char *, int);
int	 carp_demote_reset(char *, int);

/* name2id.c */
u_int16_t	 pn_name2id(const char *);
const char	*pn_id2name(u_int16_t);
void		 pn_unref(u_int16_t);
void		 pn_ref(u_int16_t);

/* snmp.c */
void	 snmp_init(struct relayd *, struct imsgev *);
int	 snmp_sendsock(struct imsgev *);
void	 snmp_hosttrap(struct table *, struct host *);

/* shuffle.c */
void		shuffle_init(struct shuffle *);
u_int16_t	shuffle_generate16(struct shuffle *);

/* log.c */
void	log_init(int);
void	log_verbose(int);
void	log_warn(const char *, ...);
void	log_warnx(const char *, ...);
void	log_info(const char *, ...);
void	log_debug(const char *, ...);
__dead void fatal(const char *);
__dead void fatalx(const char *);
