/*	$OpenBSD: smtpd.h,v 1.229 2011/07/21 23:29:24 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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

#define IMSG_SIZE_CHECK(p) do {					\
		if (IMSG_DATA_SIZE(&imsg) != sizeof(*p))	\
			fatalx("bad length imsg received");	\
	} while (0)
#define IMSG_DATA_SIZE(imsg)	((imsg)->hdr.len - IMSG_HEADER_SIZE)


#define CONF_FILE		 "/etc/mail/smtpd.conf"
#define MAX_LISTEN		 16
#define PROC_COUNT		 9
#define MAX_NAME_SIZE		 64

#define MAX_HOPS_COUNT		 100

/* sizes include the tailing '\0' */
#define MAX_LINE_SIZE		 1024
#define MAX_LOCALPART_SIZE	 128
#define MAX_DOMAINPART_SIZE	 MAXHOSTNAMELEN
#define MAX_TAG_SIZE		 32

/* return and forward path size */
#define MAX_PATH_SIZE		 256
#define MAX_RULEBUFFER_LEN	 256

#define SMTPD_QUEUE_INTERVAL	 (15 * 60)
#define SMTPD_QUEUE_MAXINTERVAL	 (4 * 60 * 60)
#define SMTPD_QUEUE_EXPIRY	 (4 * 24 * 60 * 60)
#define SMTPD_USER		 "_smtpd"
#define SMTPD_SOCKET		 "/var/run/smtpd.sock"
#define SMTPD_BANNER		 "220 %s ESMTP OpenSMTPD"
#define SMTPD_SESSION_TIMEOUT	 300
#define SMTPD_BACKLOG		 5

#define	PATH_MAILLOCAL		"/usr/libexec/mail.local"
#define	PATH_SMTPCTL		"/usr/sbin/smtpctl"

#define	DIRHASH_BUCKETS		 4096

#define PATH_SPOOL		"/var/spool/smtpd"

#define PATH_ENQUEUE		"/enqueue"
#define PATH_INCOMING		"/incoming"
#define PATH_QUEUE		"/queue"
#define PATH_PURGE		"/purge"

#define PATH_MESSAGE		"/message"
#define PATH_ENVELOPES		"/envelopes"

#define PATH_OFFLINE		"/offline"
#define PATH_BOUNCE		"/bounce"

/* number of MX records to lookup */
#define MAX_MX_COUNT		10

/* max response delay under flood conditions */
#define MAX_RESPONSE_DELAY	60

/* how many responses per state are undelayed */
#define FAST_RESPONSES		2

/* max len of any smtp line */
#define	SMTP_LINE_MAX		16384

#define F_STARTTLS		 0x01
#define F_SMTPS			 0x02
#define F_AUTH			 0x04
#define F_SSL			(F_SMTPS|F_STARTTLS)

#define F_SCERT			0x01
#define F_CCERT			0x02

#define ADVERTISE_TLS(s) \
	((s)->s_l->flags & F_STARTTLS && !((s)->s_flags & F_SECURE))

#define ADVERTISE_AUTH(s) \
	((s)->s_l->flags & F_AUTH && (s)->s_flags & F_SECURE && \
	 !((s)->s_flags & F_AUTHENTICATED))

#define SET_IF_GREATER(x,y) do { y = MAX(x,y); } while(0)
		

typedef u_int32_t	objid_t;

struct netaddr {
	struct sockaddr_storage ss;
	int bits;
};

struct relayhost {
	u_int8_t flags;
	char hostname[MAXHOSTNAMELEN];
	u_int16_t port;
	char cert[PATH_MAX];
	objid_t secmapid;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_OK,		/* answer to smtpctl requests */
	IMSG_CTL_FAIL,
	IMSG_CTL_SHUTDOWN,
	IMSG_CTL_VERBOSE,
	IMSG_CONF_START,
	IMSG_CONF_SSL,
	IMSG_CONF_LISTENER,
	IMSG_CONF_MAP,
	IMSG_CONF_MAP_CONTENT,
	IMSG_CONF_RULE,
	IMSG_CONF_RULE_SOURCE,
	IMSG_CONF_END,
	IMSG_CONF_RELOAD,
	IMSG_LKA_MAIL,
	IMSG_LKA_RCPT,
	IMSG_LKA_SECRET,
	IMSG_LKA_RULEMATCH,
	IMSG_MDA_SESS_NEW,
	IMSG_MDA_DONE,
	IMSG_MFA_RCPT,
	IMSG_MFA_MAIL,

	IMSG_QUEUE_CREATE_MESSAGE,
	IMSG_QUEUE_SUBMIT_ENVELOPE,
	IMSG_QUEUE_COMMIT_ENVELOPES,
	IMSG_QUEUE_REMOVE_MESSAGE,
	IMSG_QUEUE_COMMIT_MESSAGE,
	IMSG_QUEUE_TEMPFAIL,
	IMSG_QUEUE_PAUSE_LOCAL,
	IMSG_QUEUE_PAUSE_OUTGOING,
	IMSG_QUEUE_RESUME_LOCAL,
	IMSG_QUEUE_RESUME_OUTGOING,

	IMSG_QUEUE_MESSAGE_UPDATE,
	IMSG_QUEUE_MESSAGE_FD,
	IMSG_QUEUE_MESSAGE_FILE,
	IMSG_QUEUE_SCHEDULE,
	IMSG_QUEUE_REMOVE,

	IMSG_RUNNER_REMOVE,
	IMSG_RUNNER_SCHEDULE,

	IMSG_BATCH_CREATE,
	IMSG_BATCH_APPEND,
	IMSG_BATCH_CLOSE,
	IMSG_BATCH_DONE,

	IMSG_PARENT_ENQUEUE_OFFLINE,
	IMSG_PARENT_FORWARD_OPEN,
	IMSG_PARENT_FORK_MDA,

	IMSG_PARENT_AUTHENTICATE,
	IMSG_PARENT_SEND_CONFIG,

	IMSG_STATS,
	IMSG_SMTP_ENQUEUE,
	IMSG_SMTP_PAUSE,
	IMSG_SMTP_RESUME,

	IMSG_DNS_HOST,
	IMSG_DNS_HOST_END,
	IMSG_DNS_MX,
	IMSG_DNS_PTR
};

enum blockmodes {
	BM_NORMAL,
	BM_NONBLOCK
};

struct imsgev {
	struct imsgbuf		 ibuf;
	void			(*handler)(int, short, void *);
	struct event		 ev;
	void			*data;
	int			 proc;
	short			 events;
};

struct ctl_conn {
	TAILQ_ENTRY(ctl_conn)	 entry;
	u_int8_t		 flags;
#define CTL_CONN_NOTIFY		 0x01
	struct imsgev		 iev;
};
TAILQ_HEAD(ctl_connlist, ctl_conn);

struct ctl_id {
	objid_t		 id;
	char		 name[MAX_NAME_SIZE];
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
	PROC_RUNNER,
} smtpd_process;

struct peer {
	enum smtp_proc_type	 id;
	void			(*cb)(int, short, void *);
};

enum map_type {
	T_SINGLE,
	T_LIST,
	T_HASH
};

enum map_src {
	S_NONE,
	S_DYN,
	S_DNS,
	S_PLAIN,
	S_DB,
	S_EXT
};

enum map_kind {
	K_NONE,
	K_ALIAS,
	K_VIRTUAL,
	K_SECRET
};	

enum mapel_type {
	ME_STRING,
	ME_NET,
	ME_NETMASK
};

struct mapel {
	TAILQ_ENTRY(mapel)		 me_entry;
	union mapel_data {
		char			 med_string[MAX_LINE_SIZE];
		struct netaddr		 med_addr;
	}				 me_key;
	union mapel_data		 me_val;
};

struct map {
	TAILQ_ENTRY(map)		 m_entry;
#define F_USED				 0x01
#define F_DYNAMIC			 0x02
	u_int8_t			 m_flags;
	char				 m_name[MAX_LINE_SIZE];
	objid_t				 m_id;
	enum map_type			 m_type;
	enum mapel_type			 m_eltype;
	enum map_src			 m_src;
	char				 m_config[MAXPATHLEN];
	TAILQ_HEAD(mapel_list, mapel)	 m_contents;
};


struct map_backend {
	void *(*open)(char *);
	void (*close)(void *);
	void *(*lookup)(void *, char *, enum map_kind);
};


enum cond_type {
	C_ALL,
	C_NET,
	C_DOM,
	C_VDOM
};

struct cond {
	TAILQ_ENTRY(cond)		 c_entry;
	objid_t				 c_map;
	enum cond_type			 c_type;
};

enum action_type {
	A_INVALID,
	A_RELAY,
	A_RELAYVIA,
	A_MAILDIR,
	A_MBOX,
	A_FILENAME,
	A_EXT
};

#define IS_MAILBOX(x)	((x).r_action == A_MAILDIR || (x).r_action == A_MBOX || (x).r_action == A_FILENAME)
#define IS_RELAY(x)	((x).r_action == A_RELAY || (x).r_action == A_RELAYVIA)
#define IS_EXT(x)	((x).r_action == A_EXT)

struct rule {
	TAILQ_ENTRY(rule)		 r_entry;
	char				 r_tag[MAX_TAG_SIZE];
	int				 r_accept;
	struct map			*r_sources;
	struct cond			 r_condition;
	enum action_type		 r_action;
	union rule_dest {
		char			 buffer[MAX_RULEBUFFER_LEN];
		struct relayhost       	 relayhost;
	}				 r_value;

	char				*r_user;
	struct mailaddr			*r_as;
	objid_t				 r_amap;
	time_t				 r_qexpire;
};

struct mailaddr {
	char	user[MAX_LOCALPART_SIZE];
	char	domain[MAX_DOMAINPART_SIZE];
};


enum delivery_type {
	D_INVALID = 0,
	D_MDA,
	D_MTA,
	D_BOUNCE
};

enum delivery_status {
	DS_PERMFAILURE	= 0x2,
	DS_TEMPFAILURE	= 0x4,
	DS_REJECTED	= 0x8,
	DS_ACCEPTED	= 0x10,
	DS_RETRY       	= 0x20,
	DS_EDNS		= 0x40,
	DS_ECONNECT	= 0x80
};

enum delivery_flags {
	DF_RESOLVED		= 0x1,
	DF_SCHEDULED		= 0x2,
	DF_PROCESSING		= 0x4,
	DF_AUTHENTICATED	= 0x8,
	DF_ENQUEUED		= 0x10,
	DF_FORCESCHEDULE	= 0x20,
	DF_BOUNCE		= 0x40,
	DF_INTERNAL		= 0x80 /* internal expansion forward */
};

union delivery_data {
	char user[MAXLOGNAME];
	char buffer[MAX_RULEBUFFER_LEN];
	struct mailaddr mailaddr;
};

struct delivery_mda {
	enum action_type	method;
	union delivery_data	to;
	char			as_user[MAXLOGNAME];
};

struct delivery_mta {
	struct relayhost relay;
	struct mailaddr	relay_as;
};

struct delivery {
	u_int64_t			id;
	enum delivery_type		type;

	char				helo[MAXHOSTNAMELEN];
	char				hostname[MAXHOSTNAMELEN];
	char				errorline[MAX_LINE_SIZE];
	struct sockaddr_storage		ss;

	struct mailaddr			from;
	struct mailaddr			rcpt;
	struct mailaddr			rcpt_orig;

	union delivery_method {
		struct delivery_mda	mda;
		struct delivery_mta	mta;
	} agent;

	time_t				 creation;
	time_t				 lasttry;
	time_t				 expire;
	u_int8_t			 retry;
	enum delivery_flags		 flags;
	enum delivery_status		 status;
};

enum expand_type {
	EXPAND_INVALID,
	EXPAND_USERNAME,
	EXPAND_FILENAME,
	EXPAND_FILTER,
	EXPAND_INCLUDE,
	EXPAND_ADDRESS
};

enum expand_flags {
	F_EXPAND_NONE,
	F_EXPAND_DONE
};

struct expandnode {
	RB_ENTRY(expandnode)	entry;
	size_t			refcnt;
	enum expand_flags      	flags;
	enum expand_type       	type;
	char			as_user[MAXLOGNAME];
	union delivery_data    	u;
};

RB_HEAD(expandtree, expandnode);


struct envelope {
	TAILQ_ENTRY(envelope)		entry;

	char				tag[MAX_TAG_SIZE];
	struct rule			rule;

	u_int64_t			session_id;
	u_int64_t			batch_id;

	struct delivery			delivery;
};
TAILQ_HEAD(deliverylist, envelope);


enum child_type {
	CHILD_INVALID,
	CHILD_DAEMON,
	CHILD_MDA,
	CHILD_ENQUEUE_OFFLINE
};

struct child {
	SPLAY_ENTRY(child)	 entry;
	pid_t			 pid;
	enum child_type		 type;
	enum smtp_proc_type	 title;
	int			 mda_out;
	u_int32_t		 mda_id;
};

enum session_state {
	S_INVALID = 0,
	S_INIT,
	S_GREETED,
	S_TLS,
	S_AUTH_INIT,
	S_AUTH_USERNAME,
	S_AUTH_PASSWORD,
	S_AUTH_FINALIZE,
	S_HELO,
	S_MAIL_MFA,
	S_MAIL_QUEUE,
	S_MAIL,
	S_RCPT_MFA,
	S_RCPT,
	S_DATA,
	S_DATA_QUEUE,
	S_DATACONTENT,
	S_DONE,
	S_QUIT
};
#define STATE_COUNT	18

struct ssl {
	SPLAY_ENTRY(ssl)	 ssl_nodes;
	char			 ssl_name[PATH_MAX];
	char			*ssl_cert;
	off_t			 ssl_cert_len;
	char			*ssl_key;
	off_t			 ssl_key_len;
	char			*ssl_dhparams;
	off_t			 ssl_dhparams_len;
	u_int8_t		 flags;
};

struct listener {
	u_int8_t		 flags;
	int			 fd;
	struct sockaddr_storage	 ss;
	in_port_t		 port;
	struct timeval		 timeout;
	struct event		 ev;
	char			 ssl_cert_name[PATH_MAX];
	struct ssl		*ssl;
	void			*ssl_ctx;
	char			 tag[MAX_TAG_SIZE];
	TAILQ_ENTRY(listener)	 entry;
};

struct auth {
	u_int64_t	 id;
	char		 user[MAXLOGNAME];
	char		 pass[MAX_LINE_SIZE];
	int		 success;
};

enum session_flags {
	F_EHLO		= 0x1,
	F_QUIT		= 0x2,
	F_8BITMIME	= 0x4,
	F_SECURE	= 0x8,
	F_AUTHENTICATED	= 0x10,
	F_PEERHASTLS	= 0x20,
	F_PEERHASAUTH	= 0x40,
	F_WRITEONLY	= 0x80
};

struct session {
	SPLAY_ENTRY(session)		 s_nodes;
	u_int64_t			 s_id;

	enum session_flags		 s_flags;
	enum session_state		 s_state;
	int				 s_fd;
	struct sockaddr_storage		 s_ss;
	char				 s_hostname[MAXHOSTNAMELEN];
	struct event			 s_ev;
	struct bufferevent		*s_bev;
	struct listener			*s_l;
	void				*s_ssl;
	u_char				*s_buf;
	int				 s_buflen;
	struct timeval			 s_tv;
	struct envelope			 s_msg;
	short				 s_nresp[STATE_COUNT];
	size_t				 rcptcount;
	long				 s_datalen;

	struct auth			 s_auth;

	FILE				*datafp;
	int				 mboxfd;
	int				 messagefd;
};


/* ram-queue structures */
struct ramqueue_host {
	RB_ENTRY(ramqueue_host)		host_entry;
	TAILQ_HEAD(,ramqueue_batch)	batch_queue;
	u_int64_t			h_id;
	char				hostname[MAXHOSTNAMELEN];
};
struct ramqueue_batch {
	TAILQ_ENTRY(ramqueue_batch)	batch_entry;
	TAILQ_HEAD(,ramqueue_envelope)	envelope_queue;
	enum delivery_type		type;
	u_int64_t			h_id;
	u_int64_t			b_id;
	u_int32_t      			msgid;
	struct rule			rule;
};
struct ramqueue_envelope {
	TAILQ_ENTRY(ramqueue_envelope)	 queue_entry;
	TAILQ_ENTRY(ramqueue_envelope)	 batchqueue_entry;
	struct ramqueue_host		*host;
	struct ramqueue_batch		*batch;
	u_int64_t      			 evpid;
	time_t				 sched;
};

struct ramqueue {
	struct ramqueue_envelope	       *current_evp;
	RB_HEAD(hosttree, ramqueue_host)	hosttree;
	TAILQ_HEAD(,ramqueue_envelope)		queue;
};


struct smtpd {
	char					 sc_conffile[MAXPATHLEN];
	size_t					 sc_maxsize;

#define SMTPD_OPT_VERBOSE			 0x00000001
#define SMTPD_OPT_NOACTION			 0x00000002
	u_int32_t				 sc_opts;
#define SMTPD_CONFIGURING			 0x00000001
#define SMTPD_EXITING				 0x00000002
#define SMTPD_MDA_PAUSED		       	 0x00000004
#define SMTPD_MTA_PAUSED		       	 0x00000008
#define SMTPD_SMTP_PAUSED		       	 0x00000010
	u_int32_t				 sc_flags;
	struct timeval				 sc_qintval;
	int					 sc_qexpire;
	u_int32_t				 sc_maxconn;
	struct event				 sc_ev;
	int					 *sc_pipes[PROC_COUNT]
							[PROC_COUNT];
	struct imsgev				*sc_ievs[PROC_COUNT];
	int					 sc_instances[PROC_COUNT];
	int					 sc_instance;
	char					*sc_title[PROC_COUNT];
	struct passwd				*sc_pw;
	char					 sc_hostname[MAXHOSTNAMELEN];
	struct ramqueue				 sc_rqueue;
	struct queue_backend			*sc_queue;

	TAILQ_HEAD(listenerlist, listener)	*sc_listeners;
	TAILQ_HEAD(maplist, map)		*sc_maps, *sc_maps_reload;
	TAILQ_HEAD(rulelist, rule)		*sc_rules, *sc_rules_reload;
	SPLAY_HEAD(sessiontree, session)	 sc_sessions;
	SPLAY_HEAD(msgtree, envelope)		 sc_messages;
	SPLAY_HEAD(ssltree, ssl)		*sc_ssl;
	SPLAY_HEAD(childtree, child)		 children;
	SPLAY_HEAD(lkatree, lka_session)	 lka_sessions;
	SPLAY_HEAD(mtatree, mta_session)	 mta_sessions;
	LIST_HEAD(mdalist, mda_session)		 mda_sessions;

	struct stats				*stats;
};

struct s_parent {
	time_t		start;
};

struct s_queue {
	size_t		inserts_local;
	size_t		inserts_remote;
};

struct s_runner {
	size_t		active;
	size_t		maxactive;
	size_t		bounces_active;
	size_t		bounces_maxactive;
	size_t		bounces;
};

struct s_session {
	size_t		sessions;
	size_t		sessions_inet4;
	size_t		sessions_inet6;
	size_t		sessions_active;
	size_t		sessions_maxactive;

	size_t		smtps;
	size_t		smtps_active;
	size_t		smtps_maxactive;

	size_t		starttls;
	size_t		starttls_active;
	size_t		starttls_maxactive;

	size_t		read_error;
	size_t		read_timeout;
	size_t		read_eof;
	size_t		write_error;
	size_t		write_timeout;
	size_t		write_eof;
	size_t		toofast;
	size_t		tempfail;
	size_t		linetoolong;
	size_t		delays;
};

struct s_mda {
	size_t		sessions;
	size_t		sessions_active;
	size_t		sessions_maxactive;
};

struct s_control {
	size_t		sessions;
	size_t		sessions_active;
	size_t		sessions_maxactive;
};

struct s_lka {
	size_t		queries;
	size_t		queries_active;
	size_t		queries_maxactive;
	size_t		queries_mx;
	size_t		queries_host;
	size_t		queries_cname;
	size_t		queries_failure;
};

struct s_ramqueue {
	size_t		hosts;
	size_t		batches;
	size_t		envelopes;
	size_t		hosts_max;
	size_t		batches_max;
	size_t		envelopes_max;
};

struct stats {
	struct s_parent		 parent;
	struct s_queue		 queue;
	struct s_runner		 runner;
	struct s_session	 mta;
	struct s_mda		 mda;
	struct s_session	 smtp;
	struct s_control	 control;
	struct s_lka		 lka;
	struct s_ramqueue	 ramqueue;
};

struct reload {
	int			fd;
	int			ret;
};

struct submit_status {
	u_int64_t			 id;
	int				 code;
	union submit_path {
		struct mailaddr		 maddr;
		u_int32_t		 msgid;
		u_int64_t		 evpid;
		char			 errormsg[MAX_LINE_SIZE];
	}				 u;
	enum delivery_flags		 flags;
	struct sockaddr_storage		 ss;
	struct envelope			 envelope;
};

struct forward_req {
	u_int64_t			 id;
	u_int8_t			 status;
	char				 as_user[MAXLOGNAME];
	struct envelope			 envelope;
};

enum dns_status {
	DNS_OK = 0,
	DNS_RETRY,
	DNS_EINVAL,
	DNS_ENONAME,
	DNS_ENOTFOUND,
};

struct dns {
	u_int64_t		 id;
	char			 host[MAXHOSTNAMELEN];
	int			 port;
	int			 error;
	int			 type;
	struct imsgev		*asker;
	struct sockaddr_storage	 ss;
	struct dns		*next;
};

struct secret {
	u_int64_t		 id;
	objid_t			 secmapid;
	char			 host[MAXHOSTNAMELEN];
	char			 secret[MAX_LINE_SIZE];
};

struct mda_session {
	LIST_ENTRY(mda_session)	 entry;
	struct envelope		 msg;
	struct msgbuf		 w;
	struct event		 ev;
	u_int32_t		 id;
	FILE			*datafp;
};

struct deliver {
	char			to[PATH_MAX];
	char			user[MAXLOGNAME];
	short			mode;
};

struct rulematch {
	u_int64_t		 id;
	struct submit_status	 ss;
};

enum lka_session_flags {
	F_ERROR		= 0x1
};

struct lka_session {
	SPLAY_ENTRY(lka_session)	 nodes;
	u_int64_t			 id;

	struct deliverylist		 deliverylist;
	struct expandtree		 expandtree;

	u_int8_t			 iterations;
	u_int32_t			 pending;
	enum lka_session_flags		 flags;
	struct submit_status		 ss;
};

enum mta_state {
	MTA_INVALID_STATE,
	MTA_INIT,
	MTA_SECRET,
	MTA_DATA,
	MTA_MX,
	MTA_CONNECT,
	MTA_PTR,
	MTA_PROTOCOL,
	MTA_DONE
};

/* mta session flags */
#define	MTA_FORCE_ANYSSL	0x01
#define	MTA_FORCE_SMTPS		0x02
#define	MTA_ALLOW_PLAIN		0x04
#define	MTA_USE_AUTH		0x08
#define	MTA_FORCE_MX		0x10

struct mta_relay {
	TAILQ_ENTRY(mta_relay)	 entry;
	struct sockaddr_storage	 sa;
	char			 fqdn[MAXHOSTNAMELEN];
	int			 used;
};

struct mta_session {
	SPLAY_ENTRY(mta_session) entry;
	u_int64_t		 id;
	enum mta_state		 state;
	char			*host;
	int			 port;
	int			 flags;
	TAILQ_HEAD(,envelope)	 recipients;
	TAILQ_HEAD(,mta_relay)	 relays;
	objid_t			 secmapid;
	char			*secret;
	int			 fd;
	FILE			*datafp;
	struct event		 ev;
	char			*cert;
	void			*pcb;
	struct ramqueue_batch	*batch;
};


/* maps return structures */
struct map_secret {
	char username[MAX_LINE_SIZE];
	char password[MAX_LINE_SIZE];
};

struct map_alias {
	size_t			nbnodes;
	struct expandtree	expandtree;
};

struct map_virtual {
	size_t			nbnodes;
	struct expandtree	expandtree;
};


/* queue structures */
enum queue_type {
	QT_INVALID=0,
	QT_FS
};

enum queue_kind {
	Q_INVALID=0,
	Q_ENQUEUE,
	Q_INCOMING,
	Q_QUEUE,
	Q_PURGE,
	Q_OFFLINE,
	Q_BOUNCE
};

enum queue_op {
	QOP_INVALID=0,
	QOP_CREATE,
	QOP_DELETE,
	QOP_UPDATE,
	QOP_COMMIT,
	QOP_LOAD,
	QOP_FD_R,
	QOP_FD_RW,
	QOP_PURGE
};

struct queue_backend {
	enum queue_type	type;
	int (*init)(void);
	int (*message)(enum queue_kind, enum queue_op, u_int32_t *);
	int (*envelope)(enum queue_kind, enum queue_op, struct envelope *);
};


/* auth structures */
enum auth_type {
	AUTH_INVALID=0,
	AUTH_BSD,
	AUTH_GETPWNAM,
};

struct auth_backend {
	enum auth_type	type;
	int (*authenticate)(char *, char *);
};


/* user structures */
enum user_type {
	USER_INVALID=0,
	USER_GETPWNAM,
};

#define	MAXPASSWORDLEN	128
struct user {
	char username[MAXLOGNAME];
	char directory[MAXPATHLEN];
	char password[MAXPASSWORDLEN];
	uid_t uid;
	gid_t gid;
};

struct user_backend {
	enum user_type	type;
	int (*getbyname)(struct user *, char *);
	int (*getbyuid)(struct user *, uid_t);
};


extern struct smtpd	*env;
extern void (*imsg_callback)(struct imsgev *, struct imsg *);


/* aliases.c */
int aliases_exist(objid_t, char *);
int aliases_get(objid_t, struct expandtree *, char *);
int aliases_vdomain_exists(objid_t, char *);
int aliases_virtual_exist(objid_t, struct mailaddr *);
int aliases_virtual_get(objid_t, struct expandtree *, struct mailaddr *);
int alias_parse(struct expandnode *, char *);


/* auth_backend.c */
struct auth_backend *auth_backend_lookup(enum auth_type);


/* bounce.c */
int bounce_session(int, struct envelope *);
int bounce_session_switch(FILE *, enum session_state *, char *, struct envelope *);
void bounce_event(int, short, void *);


/* config.c */
#define PURGE_LISTENERS		0x01
#define PURGE_MAPS		0x02
#define PURGE_RULES		0x04
#define PURGE_SSL		0x08
#define PURGE_EVERYTHING	0xff
void purge_config(u_int8_t);
void unconfigure(void);
void configure(void);
void init_pipes(void);
void config_pipes(struct peer *, u_int);
void config_peers(struct peer *, u_int);


/* control.c */
pid_t control(void);
void session_socket_blockmode(int, enum blockmodes);
void session_socket_no_linger(int);
int session_socket_error(int);


/* dns.c */
void dns_query_host(char *, int, u_int64_t);
void dns_query_mx(char *, int, u_int64_t);
void dns_query_ptr(struct sockaddr_storage *, u_int64_t);
void dns_async(struct imsgev *, int, struct dns *);


/* enqueue.c */
int		 enqueue(int, char **);
int		 enqueue_offline(int, char **);


/* expand.c */
int expand_cmp(struct expandnode *, struct expandnode *);
void expandtree_increment_node(struct expandtree *, struct expandnode *);
void expandtree_decrement_node(struct expandtree *, struct expandnode *);
void expandtree_remove_node(struct expandtree *, struct expandnode *);
struct expandnode *expandtree_lookup(struct expandtree *, struct expandnode *);
void expandtree_free_nodes(struct expandtree *);
RB_PROTOTYPE(expandtree, expandnode, nodes, expand_cmp);


/* forward.c */
int forwards_get(int, struct expandtree *, char *);


/* lka.c */
pid_t lka(void);
int lka_session_cmp(struct lka_session *, struct lka_session *);
SPLAY_PROTOTYPE(lkatree, lka_session, nodes, lka_session_cmp);

/* lka_session.c */
struct lka_session *lka_session_init(struct submit_status *);
void lka_session_fail(struct lka_session *);
void lka_session_destroy(struct lka_session *);


/* map.c */
void *map_lookup(objid_t, char *, enum map_kind);
struct map *map_find(objid_t);
struct map *map_findbyname(const char *);


/* mda.c */
pid_t mda(void);


/* mfa.c */
pid_t mfa(void);


/* mta.c */
pid_t mta(void);
int mta_session_cmp(struct mta_session *, struct mta_session *);
SPLAY_PROTOTYPE(mtatree, mta_session, entry, mta_session_cmp);


/* parse.y */
int parse_config(struct smtpd *, const char *, int);
int cmdline_symset(char *);


/* queue.c */
pid_t queue(void);
void queue_submit_envelope(struct envelope *);
void queue_commit_envelopes(struct envelope *);


/* queue_backend.c */
struct queue_backend *queue_backend_lookup(enum queue_type);
int queue_message_create(enum queue_kind, u_int32_t *);
int queue_message_delete(enum queue_kind, u_int32_t);
int queue_message_commit(enum queue_kind, u_int32_t);
int queue_message_fd_r(enum queue_kind, u_int32_t);
int queue_message_fd_rw(enum queue_kind, u_int32_t);
int queue_message_purge(enum queue_kind, u_int32_t);
int queue_envelope_create(enum queue_kind, struct envelope *);
int queue_envelope_delete(enum queue_kind, struct envelope *);
int queue_envelope_load(enum queue_kind, u_int64_t, struct envelope *);
int queue_envelope_update(enum queue_kind, struct envelope *);


/* queue_shared.c */
void queue_message_update(struct envelope *);
struct qwalk	*qwalk_new(char *);
int qwalk(struct qwalk *, char *);
void qwalk_close(struct qwalk *);
int bounce_record_message(struct envelope *, struct envelope *);
void show_queue(char *, int);


/* ramqueue.c */
void ramqueue_init(struct ramqueue *);
int ramqueue_load(struct ramqueue *, time_t *);
int ramqueue_load_offline(struct ramqueue *);
int ramqueue_host_cmp(struct ramqueue_host *, struct ramqueue_host *);
void ramqueue_remove(struct ramqueue *, struct ramqueue_envelope *);
int ramqueue_is_empty(struct ramqueue *);
int ramqueue_is_empty(struct ramqueue *);
int ramqueue_batch_is_empty(struct ramqueue_batch *);
int ramqueue_host_is_empty(struct ramqueue_host *);
void ramqueue_remove_batch(struct ramqueue_host *, struct ramqueue_batch *);
void ramqueue_remove_host(struct ramqueue *, struct ramqueue_host *);
void ramqueue_reschedule(struct ramqueue *, u_int64_t);
struct ramqueue_envelope *ramqueue_envelope_by_id(struct ramqueue *, u_int64_t);
struct ramqueue_envelope *ramqueue_first_envelope(struct ramqueue *);
struct ramqueue_envelope *ramqueue_next_envelope(struct ramqueue *);
struct ramqueue_envelope *ramqueue_batch_first_envelope(struct ramqueue_batch *);
RB_PROTOTYPE(hosttree, ramqueue_host, host_entry, ramqueue_host_cmp);


/* runner.c */
pid_t runner(void);
void message_reset_flags(struct envelope *);


/* smtp.c */
pid_t smtp(void);
void smtp_resume(void);


/* smtp_session.c */
void session_init(struct listener *, struct session *);
int session_cmp(struct session *, struct session *);
void session_pickup(struct session *, struct submit_status *);
void session_destroy(struct session *);
void session_respond(struct session *, char *, ...)
	__attribute__ ((format (printf, 2, 3)));
void session_bufferevent_new(struct session *);
SPLAY_PROTOTYPE(sessiontree, session, s_nodes, session_cmp);


/* smtpd.c */
int	 child_cmp(struct child *, struct child *);
void imsg_event_add(struct imsgev *);
void imsg_compose_event(struct imsgev *, u_int16_t, u_int32_t, pid_t,
    int, void *, u_int16_t);
void imsg_dispatch(int, short, void *);
SPLAY_PROTOTYPE(childtree, child, entry, child_cmp);


/* ssl.c */
void ssl_init(void);
void ssl_transaction(struct session *);
void ssl_session_init(struct session *);
void ssl_session_destroy(struct session *);
int ssl_load_certfile(const char *, u_int8_t);
void ssl_setup(struct listener *);
int ssl_cmp(struct ssl *, struct ssl *);
SPLAY_PROTOTYPE(ssltree, ssl, ssl_nodes, ssl_cmp);


/* ssl_privsep.c */
int	 ssl_ctx_use_private_key(void *, char *, off_t);
int	 ssl_ctx_use_certificate_chain(void *, char *, off_t);


/* user_backend.c */
struct user_backend *user_backend_lookup(enum user_type);


/* util.c */
typedef struct arglist arglist;
struct arglist {
	char    **list;
	u_int   num;
	u_int   nalloc;
};
void addargs(arglist *, char *, ...)
	__attribute__((format(printf, 2, 3)));
int bsnprintf(char *, size_t, const char *, ...)
	__attribute__ ((format (printf, 3, 4)));
int safe_fclose(FILE *);
int hostname_match(char *, char *);
int email_to_mailaddr(struct mailaddr *, char *);
int valid_localpart(char *);
int valid_domainpart(char *);
char *ss_to_text(struct sockaddr_storage *);
int valid_message_id(char *);
int valid_message_uid(char *);
char *time_to_text(time_t);
int secure_file(int, char *, char *, uid_t, int);
void lowercase(char *, char *, size_t);
void envelope_set_errormsg(struct envelope *, char *, ...);
char *envelope_get_errormsg(struct envelope *);
void sa_set_port(struct sockaddr *, int);
u_int64_t generate_uid(void);
void fdlimit(double);
int availdesc(void);
u_int32_t evpid_to_msgid(u_int64_t);
u_int64_t msgid_to_evpid(u_int32_t);
u_int32_t filename_to_msgid(char *);
u_int64_t filename_to_evpid(char *);
