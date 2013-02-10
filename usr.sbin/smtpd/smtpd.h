/*	$OpenBSD: smtpd.h,v 1.404 2013/02/10 15:01:16 eric Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

#include "smtpd-api.h"
#include "ioev.h"
#include "iobuf.h"

#define CONF_FILE		 "/etc/mail/smtpd.conf"
#define MAX_LISTEN		 16
#define PROC_COUNT		 10
#define MAX_NAME_SIZE		 64

#define MAX_HOPS_COUNT		 100
#define	DEFAULT_MAX_BODY_SIZE	(35*1024*1024)

#define MAX_TAG_SIZE		 32

#define	MAX_TABLE_BACKEND_SIZE	 32

/* return and forward path size */
#define	MAX_FILTER_NAME		 32
#define MAX_PATH_SIZE		 256
/*#define MAX_RULEBUFFER_LEN	 512*/
#define	EXPAND_BUFFER		 1024

#define SMTPD_QUEUE_INTERVAL	 (15 * 60)
#define SMTPD_QUEUE_MAXINTERVAL	 (4 * 60 * 60)
#define SMTPD_QUEUE_EXPIRY	 (4 * 24 * 60 * 60)
#define SMTPD_USER		 "_smtpd"
#define SMTPD_FILTER_USER	 "_smtpf"
#define SMTPD_QUEUE_USER	 "_smtpq"
#define SMTPD_SOCKET		 "/var/run/smtpd.sock"
#ifndef SMTPD_NAME
#define	SMTPD_NAME		 "OpenSMTPD"
#endif
#define SMTPD_BANNER		 "220 %s ESMTP %s"
#define SMTPD_SESSION_TIMEOUT	 300
#define SMTPD_BACKLOG		 5

#define	PATH_SMTPCTL		"/usr/sbin/smtpctl"

#define PATH_SPOOL		"/var/spool/smtpd"
#define PATH_OFFLINE		"/offline"
#define PATH_PURGE		"/purge"
#define PATH_TEMPORARY		"/temporary"
#define PATH_INCOMING		"/incoming"
#define PATH_MESSAGE		"/message"

#define	PATH_FILTERS		"/usr/libexec/smtpd"


/* number of MX records to lookup */
#define MAX_MX_COUNT		10

/* max response delay under flood conditions */
#define MAX_RESPONSE_DELAY	60

/* how many responses per state are undelayed */
#define FAST_RESPONSES		2

/* max len of any smtp line */
#define	SMTP_LINE_MAX		MAX_LINE_SIZE

#define F_STARTTLS		0x01
#define F_SMTPS			0x02
#define	F_TLS_OPTIONAL		0x04
#define F_SSL		       (F_STARTTLS | F_SMTPS)
#define F_AUTH			0x08
#define	F_BACKUP		0x10	/* XXX - MUST BE SYNC-ED WITH RELAY_BACKUP */
#define	F_STARTTLS_REQUIRE	0x20
#define	F_AUTH_REQUIRE		0x40

#define F_SCERT			0x01
#define F_CCERT			0x02

/* must match F_* for mta */
#define RELAY_STARTTLS		0x01
#define RELAY_SMTPS		0x02
#define	RELAY_TLS_OPTIONAL     	0x04
#define RELAY_SSL		(RELAY_STARTTLS | RELAY_SMTPS)
#define RELAY_AUTH		0x08
#define RELAY_BACKUP		0x10	/* XXX - MUST BE SYNC-ED WITH F_BACKUP */
#define RELAY_MX		0x20

typedef uint32_t	objid_t;

struct userinfo {
	char username[MAXLOGNAME];
	char directory[MAXPATHLEN];
	uid_t uid;
	gid_t gid;
};

struct netaddr {
	struct sockaddr_storage ss;
	int bits;
};

union sockaddr_any {
	struct in6_addr		in6;
	struct in_addr		in4;
};

struct relayhost {
	uint8_t flags;
	char hostname[MAXHOSTNAMELEN];
	uint16_t port;
	char cert[PATH_MAX];
	char authtable[MAX_PATH_SIZE];
	char authlabel[MAX_PATH_SIZE];
	char sourcetable[MAX_PATH_SIZE];
	char helotable[MAX_PATH_SIZE];
};

struct credentials {
	char username[MAX_LINE_SIZE];
	char password[MAX_LINE_SIZE];
};

struct destination {
	char	name[MAXHOSTNAMELEN];
};

struct source {
	union sockaddr_any	addr;
};

struct addrname {
	union sockaddr_any	addr;
	char			name[MAXHOSTNAMELEN];
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_OK,		/* answer to smtpctl requests */
	IMSG_CTL_FAIL,
	IMSG_CTL_SHUTDOWN,
	IMSG_CTL_VERBOSE,
	IMSG_CTL_PAUSE_MDA,
	IMSG_CTL_PAUSE_MTA,
	IMSG_CTL_PAUSE_SMTP,
	IMSG_CTL_RESUME_MDA,
	IMSG_CTL_RESUME_MTA,
	IMSG_CTL_RESUME_SMTP,
	IMSG_CTL_LIST_MESSAGES,
	IMSG_CTL_LIST_ENVELOPES,
	IMSG_CTL_REMOVE,
	IMSG_CTL_SCHEDULE,

	IMSG_CTL_TRACE,
	IMSG_CTL_UNTRACE,
	IMSG_CTL_PROFILE,
	IMSG_CTL_UNPROFILE,

	IMSG_CONF_START,
	IMSG_CONF_SSL,
	IMSG_CONF_LISTENER,
	IMSG_CONF_TABLE,
	IMSG_CONF_TABLE_CONTENT,
	IMSG_CONF_RULE,
	IMSG_CONF_RULE_SOURCE,
	IMSG_CONF_RULE_SENDER,
	IMSG_CONF_RULE_DESTINATION,
	IMSG_CONF_RULE_MAPPING,
	IMSG_CONF_RULE_USERS,
	IMSG_CONF_FILTER,
	IMSG_CONF_END,

	IMSG_LKA_UPDATE_TABLE,
	IMSG_LKA_EXPAND_RCPT,
	IMSG_LKA_SECRET,
	IMSG_LKA_SOURCE,
	IMSG_LKA_HELO,
	IMSG_LKA_USERINFO,
	IMSG_LKA_AUTHENTICATE,
	IMSG_LKA_SSL_INIT,
	IMSG_LKA_SSL_VERIFY_CERT,
	IMSG_LKA_SSL_VERIFY_CHAIN,
	IMSG_LKA_SSL_VERIFY,

	IMSG_DELIVERY_OK,
	IMSG_DELIVERY_TEMPFAIL,
	IMSG_DELIVERY_PERMFAIL,
	IMSG_DELIVERY_LOOP,

	IMSG_BOUNCE_INJECT,

	IMSG_MDA_DELIVER,
	IMSG_MDA_DONE,

	IMSG_MFA_REQ_CONNECT,
	IMSG_MFA_REQ_HELO,
	IMSG_MFA_REQ_MAIL,
	IMSG_MFA_REQ_RCPT,
	IMSG_MFA_REQ_DATA,
	IMSG_MFA_REQ_EOM,
	IMSG_MFA_EVENT_RSET,
	IMSG_MFA_EVENT_COMMIT,
	IMSG_MFA_EVENT_ROLLBACK,
	IMSG_MFA_EVENT_DISCONNECT,
	IMSG_MFA_SMTP_DATA,
	IMSG_MFA_SMTP_RESPONSE,

	IMSG_MTA_BATCH,
	IMSG_MTA_BATCH_ADD,
	IMSG_MTA_BATCH_END,

	IMSG_QUEUE_CREATE_MESSAGE,
	IMSG_QUEUE_SUBMIT_ENVELOPE,
	IMSG_QUEUE_COMMIT_ENVELOPES,
	IMSG_QUEUE_REMOVE_MESSAGE,
	IMSG_QUEUE_COMMIT_MESSAGE,
	IMSG_QUEUE_MESSAGE_FD,
	IMSG_QUEUE_MESSAGE_FILE,
	IMSG_QUEUE_REMOVE,
	IMSG_QUEUE_EXPIRE,
	IMSG_QUEUE_BOUNCE,

	IMSG_PARENT_FORWARD_OPEN,
	IMSG_PARENT_FORK_MDA,
	IMSG_PARENT_KILL_MDA,
	IMSG_PARENT_SEND_CONFIG,

	IMSG_SMTP_ENQUEUE_FD,

	IMSG_DNS_HOST,
	IMSG_DNS_HOST_END,
	IMSG_DNS_PTR,
	IMSG_DNS_MX,
	IMSG_DNS_MX_PREFERENCE,

	IMSG_STAT_INCREMENT,
	IMSG_STAT_DECREMENT,
	IMSG_STAT_SET,

	IMSG_DIGEST,
	IMSG_STATS,
	IMSG_STATS_GET,
};

enum blockmodes {
	BM_NORMAL,
	BM_NONBLOCK
};

enum smtp_proc_type {
	PROC_PARENT = 0,
	PROC_SMTP,
	PROC_MFA,
	PROC_LKA,
	PROC_QUEUE,
	PROC_MDA,
	PROC_MTA,
	PROC_CONTROL,
	PROC_SCHEDULER,
};

enum table_type {
	T_NONE		= 0,
	T_DYNAMIC	= 0x01,	/* table with external source	*/
	T_LIST		= 0x02,	/* table holding a list		*/
	T_HASH		= 0x04,	/* table holding a hash table	*/
};

enum table_service {
	K_NONE		= 0x00,
	K_ALIAS		= 0x01,	/* returns struct expand	*/
	K_DOMAIN	= 0x02,	/* returns struct destination	*/
	K_CREDENTIALS	= 0x04,	/* returns struct credentials	*/
	K_NETADDR	= 0x08,	/* returns struct netaddr	*/
	K_USERINFO	= 0x10,	/* returns struct userinfo	*/
	K_SOURCE	= 0x20, /* returns struct source	*/
	K_MAILADDR	= 0x40, /* returns struct mailaddr	*/
	K_ADDRNAME	= 0x80, /* returns struct addrname	*/
};

struct table {
	char				 t_name[MAX_LINE_SIZE];
	objid_t				 t_id;
	enum table_type			 t_type;
	char				 t_src[MAX_TABLE_BACKEND_SIZE];
	char				 t_config[MAXPATHLEN];

	struct dict			 t_dict;

	void				*t_handle;
	struct table_backend		*t_backend;
	void				*t_payload;
	void				*t_iter;
	char				 t_cfgtable[MAXPATHLEN];
};

struct table_backend {
	const unsigned int	services;
	int	(*config)(struct table *, const char *);
	void	*(*open)(struct table *);
	int	(*update)(struct table *);
	void	(*close)(void *);
	int	(*lookup)(void *, const char *, enum table_service, void **);
	int	(*fetch)(void *, enum table_service, char **);
};


enum dest_type {
	DEST_DOM,
	DEST_VDOM
};

enum action_type {
	A_RELAY,
	A_RELAYVIA,
	A_MAILDIR,
	A_MBOX,
	A_FILENAME,
	A_MDA
};

enum decision {
	R_REJECT,
	R_ACCEPT
};

struct rule {
	TAILQ_ENTRY(rule)		r_entry;
	enum decision			r_decision;
	char				r_tag[MAX_TAG_SIZE];
	struct table		       *r_sources;
	struct table		       *r_senders;

	enum dest_type			r_desttype;
	struct table		       *r_destination;

	enum action_type		r_action;
	union rule_dest {
		char			buffer[EXPAND_BUFFER];
		struct relayhost	relayhost;
	}				r_value;

	struct mailaddr		       *r_as;
	struct table		       *r_mapping;
	struct table		       *r_users;
	time_t				r_qexpire;
};

enum delivery_type {
	D_MDA,
	D_MTA,
	D_BOUNCE,
};

struct delivery_mda {
	enum action_type	method;
	char			usertable[MAX_PATH_SIZE];
	char			username[MAXLOGNAME];
	char			buffer[EXPAND_BUFFER];
};

struct delivery_mta {
	struct relayhost	relay;
};

enum bounce_type {
	B_ERROR,
	B_WARNING,
};

struct delivery_bounce {
	enum bounce_type	type;
	time_t			delay;
	time_t			expire;
};

enum expand_type {
	EXPAND_INVALID,
	EXPAND_USERNAME,
	EXPAND_FILENAME,
	EXPAND_FILTER,
	EXPAND_INCLUDE,
	EXPAND_ADDRESS
};

struct expandnode {
	RB_ENTRY(expandnode)	entry;
	TAILQ_ENTRY(expandnode)	tq_entry;
	enum expand_type	type;
	int			sameuser;
	int			alias;
	struct rule	       *rule;
	struct expandnode      *parent;
	unsigned int		depth;
	union {
		/*
		 * user field handles both expansion user and system user
		 * so we MUST make it large enough to fit a mailaddr user
		 */
		char		user[MAX_LOCALPART_SIZE];
		char		buffer[EXPAND_BUFFER];
		struct mailaddr	mailaddr;
	}			u;
};

struct expand {
	RB_HEAD(expandtree, expandnode)	 tree;
	TAILQ_HEAD(xnodes, expandnode)	*queue;
	int				 alias;
	size_t				 nb_nodes;
	struct rule			*rule;
	struct expandnode		*parent;
};

enum envelope_flags {
	EF_AUTHENTICATED	= 0x01,
	EF_BOUNCE		= 0x02,
	EF_INTERNAL		= 0x04, /* Internal expansion forward */

	/* runstate, not saved on disk */

	EF_PENDING		= 0x10,
	EF_INFLIGHT		= 0x20,
};

#define	SMTPD_ENVELOPE_VERSION		1
struct envelope {
	TAILQ_ENTRY(envelope)		entry;

	char				tag[MAX_TAG_SIZE];

	uint32_t			version;
	uint64_t			id;
	enum envelope_flags		flags;

	char				helo[MAXHOSTNAMELEN];
	char				hostname[MAXHOSTNAMELEN];
	char				errorline[MAX_LINE_SIZE + 1];
	struct sockaddr_storage		ss;

	struct mailaddr			sender;
	struct mailaddr			rcpt;
	struct mailaddr			dest;

	enum delivery_type		type;
	union {
		struct delivery_mda	mda;
		struct delivery_mta	mta;
		struct delivery_bounce	bounce;
	}				agent;

	uint16_t			retry;
	time_t				creation;
	time_t				expire;
	time_t				lasttry;
	time_t				nexttry;
	time_t				lastbounce;
};

enum envelope_field {
	EVP_VERSION,
	EVP_TAG,
	EVP_MSGID,
	EVP_TYPE,
	EVP_HELO,
	EVP_HOSTNAME,
	EVP_ERRORLINE,
	EVP_SOCKADDR,
	EVP_SENDER,
	EVP_RCPT,
	EVP_DEST,
	EVP_CTIME,
	EVP_EXPIRE,
	EVP_RETRY,
	EVP_LASTTRY,
	EVP_LASTBOUNCE,
	EVP_FLAGS,
	EVP_MDA_METHOD,
	EVP_MDA_BUFFER,
	EVP_MDA_USER,
	EVP_MDA_USERTABLE,
	EVP_MTA_RELAY,
	EVP_MTA_RELAY_AUTH,
	EVP_MTA_RELAY_CERT,
	EVP_MTA_RELAY_SOURCE,
	EVP_MTA_RELAY_HELO,
	EVP_BOUNCE_TYPE,
	EVP_BOUNCE_DELAY,
	EVP_BOUNCE_EXPIRE,
};

struct listener {
	uint8_t			 flags;
	int			 fd;
	struct sockaddr_storage	 ss;
	in_port_t		 port;
	struct timeval		 timeout;
	struct event		 ev;
	char			 ssl_cert_name[PATH_MAX];
	struct ssl		*ssl;
	void			*ssl_ctx;
	char			 tag[MAX_TAG_SIZE];
	char			 authtable[MAX_LINE_SIZE];
	char			 helo[MAXHOSTNAMELEN];
	TAILQ_ENTRY(listener)	 entry;
};

struct smtpd {
	char				sc_conffile[MAXPATHLEN];
	size_t				sc_maxsize;

	pid_t				sc_pid;

#define SMTPD_OPT_VERBOSE		0x00000001
#define SMTPD_OPT_NOACTION		0x00000002
	uint32_t			sc_opts;
#define SMTPD_CONFIGURING		0x00000001
#define SMTPD_EXITING			0x00000002
#define SMTPD_MDA_PAUSED		0x00000004
#define SMTPD_MTA_PAUSED		0x00000008
#define SMTPD_SMTP_PAUSED		0x00000010
#define SMTPD_MDA_BUSY			0x00000020
#define SMTPD_MTA_BUSY			0x00000040
#define SMTPD_BOUNCE_BUSY		0x00000080
#define SMTPD_SMTP_DISABLED		0x00000100
	uint32_t			sc_flags;
	uint32_t			sc_queue_flags;
#define QUEUE_COMPRESS			0x00000001
	char			       *sc_queue_compress_algo;
	int				sc_qexpire;
#define MAX_BOUNCE_WARN			4
	time_t				sc_bounce_warn[MAX_BOUNCE_WARN];
	struct event			sc_ev;
	struct passwd		       *sc_pw;
	struct passwd		       *sc_pwqueue;
	char				sc_hostname[MAXHOSTNAMELEN];
	struct scheduler_backend       *sc_scheduler;
	struct stat_backend	       *sc_stat;

	time_t					 sc_uptime;

	TAILQ_HEAD(listenerlist, listener)	*sc_listeners;

	TAILQ_HEAD(rulelist, rule)		*sc_rules, *sc_rules_reload;
	
	struct dict			       *sc_ssl_dict;

	struct dict			       *sc_tables_dict;		/* keyed lookup	*/
	struct tree			       *sc_tables_tree;		/* id lookup	*/

	struct dict				sc_filters;
	uint32_t				filtermask;
};

#define	TRACE_VERBOSE	0x0001
#define	TRACE_IMSG	0x0002
#define	TRACE_IO	0x0004
#define	TRACE_SMTP	0x0008
#define	TRACE_MFA	0x0010
#define	TRACE_MTA	0x0020
#define	TRACE_BOUNCE	0x0040
#define	TRACE_SCHEDULER	0x0080
#define	TRACE_LOOKUP	0x0100
#define	TRACE_STAT	0x0200
#define	TRACE_RULES	0x0400
#define	TRACE_IMSGSIZE	0x0800

#define PROFILE_TOSTAT	0x0001
#define PROFILE_IMSG	0x0002
#define PROFILE_QUEUE	0x0004

struct forward_req {
	uint64_t			id;
	uint8_t				status;

	char				user[MAXLOGNAME];
	uid_t				uid;
	gid_t				gid;
	char				directory[MAXPATHLEN];
};

struct deliver {
	char			to[PATH_MAX];
	char			from[PATH_MAX];
	char			user[MAXLOGNAME];
	short			mode;

	struct userinfo		userinfo;
};

struct filter {
	struct imsgproc	       *process;
	char			name[MAX_FILTER_NAME];
	char			path[MAXPATHLEN];
};

struct mta_host {
	SPLAY_ENTRY(mta_host)	 entry;
	struct sockaddr		*sa;
	char			*ptrname;
	int			 refcount;
	size_t			 nconn;
	time_t			 lastconn;
	time_t			 lastptrquery;

#define HOST_IGNORE	0x01
	int			 flags;
	int			 nerror;
};

struct mta_mx {
	TAILQ_ENTRY(mta_mx)	 entry;
	struct mta_host		*host;
	int			 preference;
};

struct mta_domain {
	SPLAY_ENTRY(mta_domain)	 entry;
	char			*name;
	int			 flags;
	TAILQ_HEAD(, mta_mx)	 mxs;
	int			 mxstatus;
	int			 refcount;
	size_t			 nconn;
	time_t			 lastconn;
	time_t			 lastmxquery;
};

struct mta_source {
	SPLAY_ENTRY(mta_source)	 entry;
	struct sockaddr		*sa;
	int			 refcount;
	size_t			 nconn;
	time_t			 lastconn;
};

struct mta_connector {
	TAILQ_ENTRY(mta_connector)	 lst_entry;
	struct mta_source		*source;
	struct mta_relay		*relay;
	struct mta_connectors		*queue;

#define CONNECTOR_FAMILY_ERROR	0x01
#define CONNECTOR_SOURCE_ERROR	0x02
#define CONNECTOR_MX_ERROR	0x04
#define CONNECTOR_ERROR		0x0f

#define CONNECTOR_LIMIT_HOST	0x10
#define CONNECTOR_LIMIT_ROUTE	0x20
#define CONNECTOR_LIMIT_SOURCE	0x40
#define CONNECTOR_LIMIT		0xf0
	int				 flags;

	int				 refcount;
	size_t				 nconn;
	time_t				 lastconn;
	time_t				 nextconn;
	time_t				 clearlimit;
};

struct mta_route {
	SPLAY_ENTRY(mta_route)	 entry;
	struct mta_source	*src;
	struct mta_host		*dst;
	int			 refcount;
	size_t			 nconn;
	time_t			 lastconn;
};

TAILQ_HEAD(mta_connectors, mta_connector);

struct mta_relay {
	SPLAY_ENTRY(mta_relay)	 entry;
	uint64_t		 id;

	struct mta_domain	*domain;
	int			 flags;
	char			*backupname;
	int			 backuppref;
	char			*sourcetable;
	uint16_t		 port;
	char			*cert;
	char			*authtable;
	char			*authlabel;
	char			*helotable;
	char			*heloname;

	char			*secret;

	size_t			 ntask;
	TAILQ_HEAD(, mta_task)	 tasks;

	struct tree		 connectors;
	size_t			 nconnector;
	size_t			 sourceloop;

	struct mta_connectors	 c_ready;
	struct mta_connectors	 c_limit;
	struct mta_connectors	 c_delay;
	struct mta_connectors	 c_error;
	struct event		 ev;

	int			 fail;
	char			*failstr;

#define RELAY_WAIT_MX		0x01
#define RELAY_WAIT_PREFERENCE	0x02
#define RELAY_WAIT_SECRET	0x04
#define RELAY_WAIT_SOURCE	0x08
#define RELAY_WAITMASK		0x0f
	int			 status;

	int			 refcount;
	size_t			 nconn;
	time_t			 lastconn;

	size_t			 maxconn;
};

struct mta_envelope {
	TAILQ_ENTRY(mta_envelope)	 entry;
	uint64_t			 id;
	time_t				 creation;
	char				*dest;
	char				*rcpt;
	struct mta_task			*task;
};

struct mta_task {
	TAILQ_ENTRY(mta_task)		 entry;
	struct mta_relay		*relay;
	uint32_t			 msgid;
	TAILQ_HEAD(, mta_envelope)	 envelopes;
	char				*sender;
};

enum queue_op {
	QOP_CREATE,
	QOP_DELETE,
	QOP_UPDATE,
	QOP_WALK,
	QOP_COMMIT,
	QOP_LOAD,
	QOP_FD_RW,
	QOP_FD_R,
	QOP_CORRUPT,
};

struct queue_backend {
	int	(*init)(int);
	int	(*message)(enum queue_op, uint32_t *);
	int	(*envelope)(enum queue_op, uint64_t *, char *, size_t);
};

struct compress_backend {
	void *	(*compress_new)(void);
	size_t	(*compress_chunk)(void *, void *, size_t, void *, size_t);
	size_t	(*compress_finalize)(void *, void *, size_t);
	void *	(*uncompress_new)(void);
	size_t	(*uncompress_chunk)(void *, void *, size_t, void *, size_t);
	size_t	(*uncompress_finalize)(void *, void *, size_t);
};

/* auth structures */
enum auth_type {
	AUTH_BSD,
	AUTH_PWD,
};

struct auth_backend {
	int	(*authenticate)(char *, char *);
};


/* delivery_backend */
struct delivery_backend {
	int	allow_root;
	void	(*open)(struct deliver *);
};

struct evpstate {
	uint64_t		evpid;
	uint16_t		flags;
	uint16_t		retry;
	time_t			time;
};

struct scheduler_info {
	uint64_t		evpid;
	enum delivery_type	type;
	uint16_t		retry;
	time_t			creation;
	time_t			expire;
	time_t			lasttry;
	time_t			lastbounce;
	time_t			nexttry;
};

#define SCHED_NONE		0x00
#define SCHED_DELAY		0x01
#define SCHED_REMOVE		0x02
#define SCHED_EXPIRE		0x04
#define SCHED_BOUNCE		0x08
#define SCHED_MDA		0x10
#define SCHED_MTA		0x20

struct scheduler_batch {
	int		 type;
	time_t		 delay;
	size_t		 evpcount;
	uint64_t	*evpids;
};

struct scheduler_backend {
	void	(*init)(void);

	void	(*insert)(struct scheduler_info *);
	size_t	(*commit)(uint32_t);
	size_t	(*rollback)(uint32_t);

	void	(*update)(struct scheduler_info *);
	void	(*delete)(uint64_t);

	void	(*batch)(int, struct scheduler_batch *);

	size_t	(*messages)(uint32_t, uint32_t *, size_t);
	size_t	(*envelopes)(uint64_t, struct evpstate *, size_t);
	void	(*schedule)(uint64_t);
	void	(*remove)(uint64_t);
};


enum stat_type {
	STAT_COUNTER,
	STAT_TIMESTAMP,
	STAT_TIMEVAL,
	STAT_TIMESPEC,
};

struct stat_value {
	enum stat_type	type;
	union stat_v {
		size_t		counter;
		time_t		timestamp;
		struct timeval	tv;
		struct timespec	ts;
	} u;
};

#define	STAT_KEY_SIZE	1024
struct stat_kv {
	void	*iter;
	char	key[STAT_KEY_SIZE];
	struct stat_value	val;
};

struct stat_backend {
	void	(*init)(void);
	void	(*close)(void);
	void	(*increment)(const char *, size_t);
	void	(*decrement)(const char *, size_t);
	void	(*set)(const char *, const struct stat_value *);
	int	(*iter)(void **, char **, struct stat_value *);
};

struct stat_digest {
	time_t			 startup;
	time_t			 timestamp;

	size_t			 clt_connect;
	size_t			 clt_disconnect;

	size_t			 evp_enqueued;
	size_t			 evp_dequeued;

	size_t			 evp_expired;
	size_t			 evp_removed;
	size_t			 evp_bounce;

	size_t			 dlv_ok;
	size_t			 dlv_permfail;
	size_t			 dlv_tempfail;
	size_t			 dlv_loop;
};

#define MSZ_EVP		512


struct mproc {
	pid_t		 pid;
	char		*name;
	int		 proc;
	void		(*handler)(struct mproc *, struct imsg *);
	struct imsgbuf	 imsgbuf;
	struct ibuf	*ibuf;
	int		 ibuferror;
	int		 enable;
	short		 events;
	struct event	 ev;
	void		*data;

	off_t		 msg_in;
	off_t		 msg_out;
	off_t		 bytes_in;
	off_t		 bytes_out;
	size_t		 bytes_queued;
	size_t		 bytes_queued_max;
};

struct msg {
	const uint8_t	*pos;
	const uint8_t	*end;
};

extern enum smtp_proc_type	smtpd_process;

extern int verbose;
extern int profiling;

extern struct mproc *p_control;
extern struct mproc *p_parent;
extern struct mproc *p_lka;
extern struct mproc *p_mda;
extern struct mproc *p_mfa;
extern struct mproc *p_mta;
extern struct mproc *p_queue;
extern struct mproc *p_scheduler;
extern struct mproc *p_smtp;

extern struct smtpd	*env;
extern void (*imsg_callback)(struct mproc *, struct imsg *);

struct imsgproc {
	pid_t			pid;
	struct event		ev;
	struct imsgbuf	       *ibuf;
	char		       *path;
	char		       *name;
	void		      (*cb)(struct imsg *, void *);
	void		       *cb_arg;
};

/* inter-process structures */

struct bounce_req_msg {
	uint64_t		evpid;
	time_t			timestamp;
	struct delivery_bounce	bounce;
};

enum mfa_resp_status {
	MFA_OK,
	MFA_FAIL,
	MFA_CLOSE,
};

enum dns_error {
	DNS_OK = 0,
	DNS_RETRY,
	DNS_EINVAL,
	DNS_ENONAME,
	DNS_ENOTFOUND,
};

enum lka_resp_status {
	LKA_OK,
	LKA_TEMPFAIL,
	LKA_PERMFAIL
};

enum ca_resp_status {
	CA_OK,
	CA_FAIL
};

struct ca_cert_req_msg {
	uint64_t		reqid;
	char			name[MAXPATHLEN];
};

struct ca_cert_resp_msg {
	uint64_t		reqid;
	enum ca_resp_status	status;
	char		       *cert;
	off_t			cert_len;
	char		       *key;
	off_t			key_len;
};

struct ca_vrfy_req_msg {
	uint64_t		reqid;
	unsigned char  	       *cert;
	off_t			cert_len;
	size_t			n_chain;
	size_t			chain_offset;
	unsigned char	      **chain_cert;
	off_t		       *chain_cert_len;
};

struct ca_vrfy_resp_msg {
	uint64_t		reqid;
	enum ca_resp_status	status;
};


/* aliases.c */
int aliases_get(struct table *, struct expand *, const char *);
int aliases_virtual_check(struct table *, const struct mailaddr *);
int aliases_virtual_get(struct table *, struct expand *, const struct mailaddr *);
int alias_parse(struct expandnode *, const char *);


/* auth.c */
struct auth_backend *auth_backend_lookup(enum auth_type);


/* bounce.c */
void bounce_add(uint64_t);
void bounce_fd(int);


/* ca.c */
int	ca_X509_verify(void *, void *, const char *, const char *, const char **);


/* compress_backend.c */
int	compress_backend_init(const char *);
void*	compress_new(void);
size_t	compress_chunk(void *, void *, size_t, void *, size_t);
size_t	compress_finalize(void *, void *, size_t);
size_t	compress_buffer(char *, size_t, char *, size_t);
void*	uncompress_new(void);
size_t	uncompress_chunk(void *, void *, size_t, void *, size_t);
size_t	uncompress_finalize(void *, void *, size_t);
size_t	uncompress_buffer(char *, size_t, char *, size_t);
int	uncompress_file(FILE *, FILE *);

/* config.c */
#define PURGE_LISTENERS		0x01
#define PURGE_TABLES		0x02
#define PURGE_RULES		0x04
#define PURGE_SSL		0x08
#define PURGE_EVERYTHING	0xff
void purge_config(uint8_t);
void init_pipes(void);
void config_process(enum smtp_proc_type);
void config_peer(enum smtp_proc_type);
void config_done(void);


/* control.c */
pid_t control(void);


/* delivery.c */
struct delivery_backend *delivery_backend_lookup(enum action_type);


/* dns.c */
void dns_query_host(uint64_t, const char *);
void dns_query_ptr(uint64_t, const struct sockaddr *);
void dns_query_mx(uint64_t, const char *);
void dns_query_mx_preference(uint64_t, const char *, const char *);
void dns_imsg(struct mproc *, struct imsg *);


/* enqueue.c */
int		 enqueue(int, char **);
int		 enqueue_offline(int, char **);


/* envelope.c */
void envelope_set_errormsg(struct envelope *, char *, ...);
char *envelope_ascii_field_name(enum envelope_field);
int envelope_ascii_load(enum envelope_field, struct envelope *, char *);
int envelope_ascii_dump(enum envelope_field, const struct envelope *, char *,
    size_t);
int envelope_load_buffer(struct envelope *, const char *, size_t);
int envelope_dump_buffer(const struct envelope *, char *, size_t);


/* expand.c */
int expand_cmp(struct expandnode *, struct expandnode *);
void expand_insert(struct expand *, struct expandnode *);
struct expandnode *expand_lookup(struct expand *, struct expandnode *);
void expand_clear(struct expand *);
void expand_free(struct expand *);
int expand_line(struct expand *, const char *, int);
RB_PROTOTYPE(expandtree, expandnode, nodes, expand_cmp);


/* forward.c */
int forwards_get(int, struct expand *);


/* imsgproc.c */
void imsgproc_init(void);
struct imsgproc *imsgproc_fork(const char *, const char *,
    void (*)(struct imsg *, void *), void *);
void imsgproc_set_read(struct imsgproc *);
void imsgproc_set_write(struct imsgproc *);
void imsgproc_set_read_write(struct imsgproc *);
void imsgproc_reset_callback(struct imsgproc *, void (*)(struct imsg *, void *), void *);


/* lka.c */
pid_t lka(void);


/* lka_session.c */
void lka_session(uint64_t, struct envelope *);
void lka_session_forward_reply(struct forward_req *, int);


/* mda.c */
pid_t mda(void);


/* mfa.c */
pid_t mfa(void);
void mfa_ready(void);

/* mfa_session.c */
void mfa_filter_init(void);
void mfa_filter_connect(uint64_t, const struct sockaddr *,
    const struct sockaddr *, const char *);
void mfa_filter_mailaddr(uint64_t, int, const struct mailaddr *);
void mfa_filter_line(uint64_t, int, const char *);
void mfa_filter(uint64_t, int);
void mfa_filter_event(uint64_t, int);
void mfa_filter_data(uint64_t, const char *);

/* mproc.c */
int mproc_fork(struct mproc *, const char*, const char *);
void mproc_init(struct mproc *, int);
void mproc_clear(struct mproc *);
void mproc_enable(struct mproc *);
void mproc_disable(struct mproc *);
void m_compose(struct mproc *, uint32_t, uint32_t, pid_t, int, void *, size_t);
void m_composev(struct mproc *, uint32_t, uint32_t, pid_t, int,
    const struct iovec *, int);
void m_forward(struct mproc *, struct imsg *);
void m_create(struct mproc *, uint32_t, uint32_t, pid_t, int, size_t);
void m_add(struct mproc *, const void *, size_t);
void m_add_int(struct mproc *, int);
void m_add_u32(struct mproc *, uint32_t);
void m_add_time(struct mproc *, time_t);
void m_add_string(struct mproc *, const char *);
void m_add_data(struct mproc *, const void *, size_t);
void m_add_evpid(struct mproc *, uint64_t);
void m_add_msgid(struct mproc *, uint32_t);
void m_add_id(struct mproc *, uint64_t);
void m_add_sockaddr(struct mproc *, const struct sockaddr *);
void m_add_mailaddr(struct mproc *, const struct mailaddr *);
void m_add_envelope(struct mproc *, const struct envelope *);
void m_close(struct mproc *);

void m_msg(struct msg *, struct imsg *);
int  m_is_eom(struct msg *);
void m_end(struct msg *);
void m_get_int(struct msg *, int *);
void m_get_u32(struct msg *, uint32_t *);
void m_get_time(struct msg *, time_t *);
void m_get_string(struct msg *, const char **);
void m_get_data(struct msg *, const void **, size_t *);
void m_get_evpid(struct msg *, uint64_t *);
void m_get_msgid(struct msg *, uint32_t *);
void m_get_id(struct msg *, uint64_t *);
void m_get_sockaddr(struct msg *, struct sockaddr *);
void m_get_mailaddr(struct msg *, struct mailaddr *);
void m_get_envelope(struct msg *, struct envelope *);


/* mta.c */
pid_t mta(void);
void mta_route_ok(struct mta_relay *, struct mta_route *);
void mta_route_error(struct mta_relay *, struct mta_route *);
void mta_route_collect(struct mta_relay *, struct mta_route *);
void mta_source_error(struct mta_relay *, struct mta_route *, const char *);
void mta_delivery(struct mta_envelope *, const char *, int, const char *);
struct mta_task *mta_route_next_task(struct mta_relay *, struct mta_route *);
const char *mta_host_to_text(struct mta_host *);
const char *mta_relay_to_text(struct mta_relay *);

/* mta_session.c */
void mta_session(struct mta_relay *, struct mta_route *);
void mta_session_imsg(struct mproc *, struct imsg *);


/* parse.y */
int parse_config(struct smtpd *, const char *, int);
int cmdline_symset(char *);


/* queue.c */
pid_t queue(void);
void queue_ok(uint64_t);
void queue_tempfail(uint64_t, const char *);
void queue_permfail(uint64_t, const char *);
void queue_loop(uint64_t);
void queue_flow_control(void);


/* queue_backend.c */
uint32_t queue_generate_msgid(void);
uint64_t queue_generate_evpid(uint32_t);
int queue_init(const char *, int);
int queue_message_incoming_path(uint32_t, char *, size_t);
int queue_message_create(uint32_t *);
int queue_message_delete(uint32_t);
int queue_message_commit(uint32_t);
int queue_message_fd_r(uint32_t);
int queue_message_fd_rw(uint32_t);
int queue_message_corrupt(uint32_t);
int queue_envelope_create(struct envelope *);
int queue_envelope_delete(uint64_t);
int queue_envelope_load(uint64_t, struct envelope *);
int queue_envelope_update(struct envelope *);
int queue_envelope_walk(struct envelope *);


/* ruleset.c */
struct rule *ruleset_match(const struct envelope *);


/* scheduler.c */
pid_t scheduler(void);


/* scheduler_bakend.c */
struct scheduler_backend *scheduler_backend_lookup(const char *);
void scheduler_info(struct scheduler_info *, struct envelope *);
time_t scheduler_compute_schedule(struct scheduler_info *);


/* smtp.c */
pid_t smtp(void);
void smtp_collect(void);


/* smtp_session.c */
int smtp_session(struct listener *, int, const struct sockaddr_storage *,
    const char *);
void smtp_session_imsg(struct mproc *, struct imsg *);


/* smtpd.c */
void imsg_dispatch(struct mproc *, struct imsg *);
const char *proc_name(enum smtp_proc_type);
const char *proc_title(enum smtp_proc_type);
const char *imsg_to_str(int);


/* ssl_smtpd.c */
void   *ssl_mta_init(char *, off_t, char *, off_t);
void   *ssl_smtp_init(void *, char *, off_t, char *, off_t);


/* stat_backend.c */
struct stat_backend	*stat_backend_lookup(const char *);
void	stat_increment(const char *, size_t);
void	stat_decrement(const char *, size_t);
void	stat_set(const char *, const struct stat_value *);
struct stat_value *stat_counter(size_t);
struct stat_value *stat_timestamp(time_t);
struct stat_value *stat_timeval(struct timeval *);
struct stat_value *stat_timespec(struct timespec *);


/* table.c */
int	table_open(struct table *);
void	table_update(struct table *);
void	table_close(struct table *);
int	table_check_use(struct table *, uint32_t, uint32_t);
int	table_check_type(struct table *, uint32_t);
int	table_check_service(struct table *, uint32_t);
int	table_lookup(struct table *, const char *, enum table_service, void **);
int	table_fetch(struct table *, enum table_service, char **);
struct table *table_find(objid_t);
struct table *table_findbyname(const char *);
struct table *table_create(const char *, const char *, const char *);
void table_destroy(struct table *);
void table_add(struct table *, const char *, const char *);
void table_delete(struct table *, const char *);
void table_delete_all(struct table *);
void table_replace(struct table *, struct table *);
int table_domain_match(const char *, const char *);
int table_netaddr_match(const char *, const char *);
int table_mailaddr_match(const char *, const char *);
void	table_open_all(void);
void	table_close_all(void);
void	table_set_payload(struct table *, void *);
void   *table_get_payload(struct table *);
void	table_set_configuration(struct table *, struct table *);
struct table	*table_get_configuration(struct table *);
const void	*table_get(struct table *, const char *);

void *table_config_create(void);
const char *table_config_get(void *, const char *);
void table_config_destroy(void *);
int table_config_parse(void *, const char *, enum table_type);


/* to.c */
int email_to_mailaddr(struct mailaddr *, char *);
uint32_t evpid_to_msgid(uint64_t);
uint64_t msgid_to_evpid(uint32_t);
int text_to_netaddr(struct netaddr *, const char *);
int text_to_mailaddr(struct mailaddr *, const char *);
int text_to_relayhost(struct relayhost *, const char *);
int text_to_userinfo(struct userinfo *, const char *);
int text_to_credentials(struct credentials *, const char *);
int text_to_expandnode(struct expandnode *, const char *);
uint64_t text_to_evpid(const char *);
uint32_t text_to_msgid(const char *);
const char *sa_to_text(const struct sockaddr *);
const char *ss_to_text(const struct sockaddr_storage *);
const char *time_to_text(time_t);
const char *duration_to_text(time_t);
const char *relayhost_to_text(const struct relayhost *);
const char *rule_to_text(struct rule *);
const char *sockaddr_to_text(struct sockaddr *);
const char *mailaddr_to_text(const struct mailaddr *);


/* util.c */
typedef struct arglist arglist;
struct arglist {
	char	**list;
	uint	  num;
	uint	  nalloc;
};
void addargs(arglist *, char *, ...)
	__attribute__((format(printf, 2, 3)));
int bsnprintf(char *, size_t, const char *, ...)
	__attribute__((format (printf, 3, 4)));
int mkdirs(char *, mode_t);
int safe_fclose(FILE *);
int hostname_match(const char *, const char *);
int valid_localpart(const char *);
int valid_domainpart(const char *);
int secure_file(int, char *, char *, uid_t, int);
int  lowercase(char *, const char *, size_t);
void xlowercase(char *, const char *, size_t);
void sa_set_port(struct sockaddr *, int);
uint64_t generate_uid(void);
void fdlimit(double);
int availdesc(void);
int ckdir(const char *, mode_t, uid_t, gid_t, int);
int rmtree(char *, int);
int mvpurge(char *, char *);
int mktmpfile(void);
const char *parse_smtp_response(char *, size_t, char **, int *);
void *xmalloc(size_t, const char *);
void *xcalloc(size_t, size_t, const char *);
char *xstrdup(const char *, const char *);
void *xmemdup(const void *, size_t, const char *);
char *strip(char *);
void iobuf_xinit(struct iobuf *, size_t, size_t, const char *);
void iobuf_xfqueue(struct iobuf *, const char *, const char *, ...);
void log_envelope(const struct envelope *, const char *, const char *,
    const char *);
void session_socket_blockmode(int, enum blockmodes);
void session_socket_no_linger(int);
int session_socket_error(int);


/* waitq.c */
int  waitq_wait(void *, void (*)(void *, void *, void *), void *);
void waitq_run(void *, void *);
