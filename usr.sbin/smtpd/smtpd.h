/*	$OpenBSD: smtpd.h,v 1.310 2012/07/12 08:51:43 chl Exp $	*/

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

#include "filter_api.h"
#include "ioev.h"
#include "iobuf.h"

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

#define MAX_TAG_SIZE		 32
/* SYNC WITH filter.h		  */
//#define MAX_LINE_SIZE		 1024
//#define MAX_LOCALPART_SIZE	 128
//#define MAX_DOMAINPART_SIZE	 MAXHOSTNAMELEN

/* return and forward path size */
#define	MAX_FILTER_NAME		 32
#define MAX_PATH_SIZE		 256
#define MAX_RULEBUFFER_LEN	 256

#define SMTPD_QUEUE_INTERVAL	 (15 * 60)
#define SMTPD_QUEUE_MAXINTERVAL	 (4 * 60 * 60)
#define SMTPD_QUEUE_EXPIRY	 (4 * 24 * 60 * 60)
#define SMTPD_USER		 "_smtpd"
#define SMTPD_FILTER_USER      	 "_smtpmfa"
#define SMTPD_SOCKET		 "/var/run/smtpd.sock"
#define SMTPD_BANNER		 "220 %s ESMTP OpenSMTPD"
#define SMTPD_SESSION_TIMEOUT	 300
#define SMTPD_BACKLOG		 5

#define	PATH_SMTPCTL		"/usr/sbin/smtpctl"

#define	DIRHASH_BUCKETS		 4096

#define PATH_SPOOL		"/var/spool/smtpd"
#define PATH_OFFLINE		"/offline"
#define PATH_PURGE		"/purge"
#define PATH_INCOMING		"/incoming"
#define PATH_ENVELOPES		"/envelopes"
#define PATH_MESSAGE		"/message"

/* number of MX records to lookup */
#define MAX_MX_COUNT		10

/* max response delay under flood conditions */
#define MAX_RESPONSE_DELAY	60

/* how many responses per state are undelayed */
#define FAST_RESPONSES		2

/* max len of any smtp line */
#define	SMTP_LINE_MAX		1024

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
	char authmap[MAX_PATH_SIZE];
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
	IMSG_CONF_FILTER,
	IMSG_CONF_END,
	IMSG_LKA_MAIL,
	IMSG_LKA_RCPT,
	IMSG_LKA_SECRET,
	IMSG_LKA_RULEMATCH,
	IMSG_MDA_SESS_NEW,
	IMSG_MDA_DONE,

	IMSG_MFA_CONNECT,
 	IMSG_MFA_HELO,
 	IMSG_MFA_MAIL,
 	IMSG_MFA_RCPT,
 	IMSG_MFA_DATALINE,
	IMSG_MFA_QUIT,
	IMSG_MFA_CLOSE,
	IMSG_MFA_RSET,

	IMSG_QUEUE_CREATE_MESSAGE,
	IMSG_QUEUE_SUBMIT_ENVELOPE,
	IMSG_QUEUE_COMMIT_ENVELOPES,
	IMSG_QUEUE_REMOVE_MESSAGE,
	IMSG_QUEUE_COMMIT_MESSAGE,
	IMSG_QUEUE_TEMPFAIL,
	IMSG_QUEUE_PAUSE_MDA,
	IMSG_QUEUE_PAUSE_MTA,
	IMSG_QUEUE_RESUME_MDA,
	IMSG_QUEUE_RESUME_MTA,

	IMSG_QUEUE_DELIVERY_OK,
	IMSG_QUEUE_DELIVERY_TEMPFAIL,
	IMSG_QUEUE_DELIVERY_PERMFAIL,
	IMSG_QUEUE_MESSAGE_FD,
	IMSG_QUEUE_MESSAGE_FILE,
	IMSG_QUEUE_SCHEDULE,
	IMSG_QUEUE_REMOVE,

	IMSG_SCHEDULER_REMOVE,
	IMSG_SCHEDULER_SCHEDULE,

	IMSG_BATCH_CREATE,
	IMSG_BATCH_APPEND,
	IMSG_BATCH_CLOSE,
	IMSG_BATCH_DONE,

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
	PROC_SCHEDULER,
} smtpd_process;

struct peer {
	enum smtp_proc_type	 id;
	void			(*cb)(int, short, void *);
};

enum map_src {
	S_NONE,
	S_PLAIN,
	S_DB
};

enum map_kind {
	K_NONE,
	K_ALIAS,
	K_VIRTUAL,
	K_CREDENTIALS,
	K_NETADDR
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
	}				 me_key;
	union mapel_data		 me_val;
};

struct map {
	TAILQ_ENTRY(map)		 m_entry;
	char				 m_name[MAX_LINE_SIZE];
	objid_t				 m_id;
	enum mapel_type			 m_eltype;
	enum map_src			 m_src;
	char				 m_config[MAXPATHLEN];
	TAILQ_HEAD(mapel_list, mapel)	 m_contents;
};


struct map_backend {
	void *(*open)(struct map *);
	void (*close)(void *);
	void *(*lookup)(void *, char *, enum map_kind);
	int  (*compare)(void *, char *, enum map_kind, int (*)(char *, char *));
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
	A_MDA
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
	DS_PERMFAILURE	= 0x1,
	DS_TEMPFAILURE	= 0x2,
};

enum delivery_flags {
	DF_AUTHENTICATED	= 0x1,
	DF_ENQUEUED		= 0x2,
	DF_BOUNCE		= 0x4,
	DF_INTERNAL		= 0x8 /* internal expansion forward */
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

#define	SMTPD_ENVELOPE_VERSION		1
struct envelope {
	TAILQ_ENTRY(envelope)		entry;

	char				tag[MAX_TAG_SIZE];
	struct rule			rule;

	u_int64_t			session_id;
	u_int64_t			batch_id;

	u_int32_t			version;
	u_int64_t			id;
	enum delivery_type		type;

	char				helo[MAXHOSTNAMELEN];
	char				hostname[MAXHOSTNAMELEN];
	char				errorline[MAX_LINE_SIZE + 1];
	struct sockaddr_storage		ss;

	struct mailaddr			sender;
	struct mailaddr			rcpt;
	struct mailaddr			dest;

	union delivery_method {
		struct delivery_mda	mda;
		struct delivery_mta	mta;
	} agent;

	time_t				 creation;
	time_t				 lasttry;
	time_t				 expire;
	u_int8_t			 retry;
	enum delivery_flags		 flags;
};
TAILQ_HEAD(deliverylist, envelope);

enum envelope_field {
	EVP_VERSION,
	EVP_ID,
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
	EVP_FLAGS,
	EVP_MDA_METHOD,
	EVP_MDA_BUFFER,
	EVP_MDA_USER,
	EVP_MTA_RELAY_HOST,
	EVP_MTA_RELAY_PORT,
	EVP_MTA_RELAY_FLAGS,
	EVP_MTA_RELAY_CERT,
	EVP_MTA_RELAY_AUTHMAP
};


enum child_type {
	CHILD_INVALID,
	CHILD_DAEMON,
	CHILD_MDA,
	CHILD_ENQUEUE_OFFLINE,
};

struct child {
	SPLAY_ENTRY(child)	 entry;
	pid_t			 pid;
	enum child_type		 type;
	enum smtp_proc_type	 title;
	int			 mda_out;
	u_int32_t		 mda_id;
	char			*path;
};

enum session_state {
	S_NEW = 0,
	S_CONNECTED,
	S_INIT,
	S_GREETED,
	S_TLS,
	S_AUTH_INIT,
	S_AUTH_USERNAME,
	S_AUTH_PASSWORD,
	S_AUTH_FINALIZE,
	S_RSET,
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
	S_QUIT,
	S_CLOSE
};
#define STATE_COUNT	22

struct ssl {
	SPLAY_ENTRY(ssl)	 ssl_nodes;
	char			 ssl_name[PATH_MAX];
	char			*ssl_ca;
	off_t			 ssl_ca_len;
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
	char		 pass[MAX_LINE_SIZE + 1];
	int		 success;
};

enum session_flags {
	F_EHLO		= 0x01,
	F_8BITMIME	= 0x02,
	F_SECURE	= 0x04,
	F_AUTHENTICATED	= 0x08,
	F_WAITIMSG	= 0x10,
	F_ZOMBIE	= 0x20,
};

struct session {
	SPLAY_ENTRY(session)		 s_nodes;
	u_int64_t			 s_id;

	struct iobuf			 s_iobuf;
	struct io			 s_io;

	enum session_flags		 s_flags;
	enum session_state		 s_state;
	struct sockaddr_storage		 s_ss;
	char				 s_hostname[MAXHOSTNAMELEN];
	struct event			 s_ev;
	struct listener			*s_l;
	void				*s_ssl;
	struct timeval			 s_tv;
	struct envelope			 s_msg;
	short				 s_nresp[STATE_COUNT];
	size_t				 rcptcount;
	long				 s_datalen;

	struct auth			 s_auth;
	int				 s_dstatus;

	FILE				*datafp;
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
#define SMTPD_MDA_BUSY			       	 0x00000020
#define SMTPD_MTA_BUSY			       	 0x00000040
#define SMTPD_BOUNCE_BUSY      		       	 0x00000080
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
	struct queue_backend			*sc_queue;
	struct scheduler_backend		*sc_scheduler;

	TAILQ_HEAD(filterlist, filter)		*sc_filters;

	TAILQ_HEAD(listenerlist, listener)	*sc_listeners;
	TAILQ_HEAD(maplist, map)		*sc_maps, *sc_maps_reload;
	TAILQ_HEAD(rulelist, rule)		*sc_rules, *sc_rules_reload;
	SPLAY_HEAD(sessiontree, session)	 sc_sessions;
	SPLAY_HEAD(ssltree, ssl)		*sc_ssl;
	SPLAY_HEAD(childtree, child)		 children;
	SPLAY_HEAD(lkatree, lka_session)	 lka_sessions;
	SPLAY_HEAD(mfatree, mfa_session)	 mfa_sessions;
	SPLAY_HEAD(mtatree, mta_session)	 mta_sessions;
	LIST_HEAD(mdalist, mda_session)		 mda_sessions;

	struct stats				*stats;
	u_int64_t				 filtermask;
};

#define	TRACE_VERBOSE	0x0001
#define	TRACE_IMSG	0x0002
#define	TRACE_IO	0x0004
#define	TRACE_SMTP	0x0008
#define	TRACE_MTA	0x0010
#define	TRACE_BOUNCE	0x0020
#define	TRACE_SCHEDULER	0x0040

enum {
	STATS_SMTP_SESSION = 0,
	STATS_SMTP_SESSION_INET4,
	STATS_SMTP_SESSION_INET6,
	STATS_SMTP_SMTPS,
	STATS_SMTP_STARTTLS,

	STATS_MTA_SESSION,

	STATS_MDA_SESSION,

	STATS_CONTROL_SESSION,

	STATS_LKA_SESSION,
	STATS_LKA_SESSION_MX,
	STATS_LKA_SESSION_HOST,
	STATS_LKA_SESSION_CNAME,
	STATS_LKA_FAILURE,

	STATS_SCHEDULER,
	STATS_SCHEDULER_BOUNCES,

	STATS_QUEUE_LOCAL,
	STATS_QUEUE_REMOTE,

	STATS_RAMQUEUE_ENVELOPE,
	STATS_RAMQUEUE_MESSAGE,
	STATS_RAMQUEUE_BATCH,
	STATS_RAMQUEUE_HOST,

	STATS_MAX,
};

#define STAT_COUNT	0
#define STAT_ACTIVE	1
#define STAT_MAXACTIVE	2

struct	stat_counter {
	size_t	count;
	size_t	active;
	size_t	maxactive;
};

struct s_parent {
	time_t		start;
};

struct s_session {
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

struct stats {
	struct s_parent		 parent;
	struct s_session	 mta;
	struct s_session	 smtp;

	struct stat_counter	 counters[STATS_MAX];
};

struct submit_status {
	u_int64_t			 id;
	int				 code;
	union submit_path {
		struct mailaddr		 maddr;
		u_int32_t		 msgid;
		u_int64_t		 evpid;
		char			 errormsg[MAX_LINE_SIZE + 1];
		char			 dataline[MAX_LINE_SIZE + 1];
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
	char			 mapname[MAX_PATH_SIZE];
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
	char			from[PATH_MAX];
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

struct filter {
	TAILQ_ENTRY(filter)     f_entry;
	pid_t			pid;
	struct event		ev;
	struct imsgbuf		*ibuf;
	char			name[MAX_FILTER_NAME];
	char			path[MAXPATHLEN];
};

struct mfa_session {
	SPLAY_ENTRY(mfa_session)	 nodes;
	u_int64_t			 id;

	enum session_state		 state;
	struct submit_status		 ss;
	struct filter			*filter;
	struct filter_msg		 fm;
};

enum mta_state {
	MTA_INIT,
	MTA_SECRET,
	MTA_DATA,
	MTA_MX,
	MTA_CONNECT,
	MTA_DONE,
	MTA_SMTP_READY,
	MTA_SMTP_BANNER,
	MTA_SMTP_EHLO,
	MTA_SMTP_HELO,
	MTA_SMTP_STARTTLS,
	MTA_SMTP_AUTH,
	MTA_SMTP_MAIL,
	MTA_SMTP_RCPT,
	MTA_SMTP_DATA,
	MTA_SMTP_QUIT,
	MTA_SMTP_BODY,
	MTA_SMTP_DONE,
	MTA_SMTP_RSET,
};

/* mta session flags */
#define	MTA_FORCE_ANYSSL	0x01
#define	MTA_FORCE_SMTPS		0x02
#define	MTA_ALLOW_PLAIN		0x04
#define	MTA_USE_AUTH		0x08
#define	MTA_FORCE_MX		0x10
#define	MTA_USE_CERT		0x20
#define	MTA_TLS			0x40

struct mta_relay {
	TAILQ_ENTRY(mta_relay)	 entry;
	struct sockaddr_storage	 sa;
	char			 fqdn[MAXHOSTNAMELEN];
	int			 used;
};

struct mta_task;

#define MTA_EXT_STARTTLS     0x01
#define MTA_EXT_AUTH         0x02
#define MTA_EXT_PIPELINING   0x04

struct mta_session {
	SPLAY_ENTRY(mta_session) entry;
	u_int64_t		 id;
	enum mta_state		 state;
	char			*host;
	int			 port;
	int			 flags;
	TAILQ_HEAD(,mta_relay)	 relays;
	char			*authmap;
	char			*secret;
	FILE			*datafp;

	TAILQ_HEAD(,mta_task)	 tasks;

	struct envelope		*currevp;
	struct iobuf		 iobuf;
	struct io		 io;
	int			 ext; /* extension */
	struct ssl		*ssl;
};

struct mta_batch {
	u_int64_t		id;
	struct relayhost	relay;

	u_int32_t		msgid;
};

/* maps return structures */
struct map_credentials {
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

struct map_netaddr {
	struct netaddr		netaddr;
};

enum queue_op {
	QOP_INVALID=0,
	QOP_CREATE,
	QOP_DELETE,
	QOP_UPDATE,
	QOP_COMMIT,
	QOP_LOAD,
	QOP_FD_R,
	QOP_CORRUPT,
};

struct queue_backend {
	int (*init)(int);
	int (*message)(enum queue_op, u_int32_t *);
	int (*envelope)(enum queue_op, struct envelope *);

	void *(*qwalk_new)(u_int32_t);
	int   (*qwalk)(void *, u_int64_t *);
	void  (*qwalk_close)(void *);
};


/* auth structures */
enum auth_type {
	AUTH_BSD,
	AUTH_PWD,
};

struct auth_backend {
	int (*authenticate)(char *, char *);
};


/* user structures */
enum user_type {
	USER_PWD,
};

#define	MAXPASSWORDLEN	128
struct mta_user {
	char username[MAXLOGNAME];
	char directory[MAXPATHLEN];
	char password[MAXPASSWORDLEN];
	uid_t uid;
	gid_t gid;
};

struct user_backend {
	int (*getbyname)(struct mta_user *, char *);
	int (*getbyuid)(struct mta_user *, uid_t);
};


/* delivery_backend */
struct delivery_backend {
	void	(*open)(struct deliver *);
};

struct scheduler_info {
	u_int64_t	evpid;
	char		destination[MAXHOSTNAMELEN];

	enum delivery_type	type;
	time_t			creation;
	time_t			lasttry;
	time_t			expire;
	u_int8_t		retry;
};

struct scheduler_backend {
	void	(*init)(void);
	int	(*setup)(void);

	int	(*next)(u_int64_t *, time_t *);

	void	(*insert)(struct scheduler_info *);
	void	(*schedule)(u_int64_t);
	void	(*remove)(u_int64_t);

	void	*(*host)(char *);
	void	*(*message)(u_int32_t);
	void	*(*batch)(u_int64_t);
	void	*(*queue)(void);
	void	 (*close)(void *);

	int	 (*fetch)(void *, u_int64_t *);
	int	 (*force)(u_int64_t);

	void	 (*display)(void);	/* may be NULL */
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


/* auth.c */
struct auth_backend *auth_backend_lookup(enum auth_type);


/* bounce.c */
int bounce_session(int, struct envelope *);
int bounce_session_switch(FILE *, enum session_state *, char *, struct envelope *);
void bounce_event(int, short, void *);
int bounce_record_message(struct envelope *, struct envelope *);

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


/* delivery.c */
struct delivery_backend *delivery_backend_lookup(enum action_type);


/* dns.c */
void dns_query_host(char *, int, u_int64_t);
void dns_query_mx(char *, int, u_int64_t);
void dns_query_ptr(struct sockaddr_storage *, u_int64_t);
void dns_async(struct imsgev *, int, struct dns *);


/* enqueue.c */
int		 enqueue(int, char **);
int		 enqueue_offline(int, char **);


/* envelope.c */
void envelope_set_errormsg(struct envelope *, char *, ...);
char *envelope_ascii_field_name(enum envelope_field);
int envelope_ascii_load(enum envelope_field, struct envelope *, char *);
int envelope_ascii_dump(enum envelope_field, struct envelope *, char *, size_t);
int envelope_load_file(struct envelope *, FILE *);
int envelope_dump_file(struct envelope *, FILE *);

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
int map_compare(objid_t, char *, enum map_kind, int (*)(char *, char *));
struct map *map_find(objid_t);
struct map *map_findbyname(const char *);


/* mda.c */
pid_t mda(void);


/* mfa.c */
pid_t mfa(void);
int mfa_session_cmp(struct mfa_session *, struct mfa_session *);
SPLAY_PROTOTYPE(mfatree, mfa_session, nodes, mfa_session_cmp);

/* mta.c */
pid_t mta(void);

/* mta_session.c */
void mta_session_imsg(struct imsgev *, struct imsg *);

/* parse.y */
int parse_config(struct smtpd *, const char *, int);
int cmdline_symset(char *);


/* queue.c */
pid_t queue(void);
void queue_submit_envelope(struct envelope *);
void queue_commit_envelopes(struct envelope *);


/* queue_backend.c */
u_int32_t queue_generate_msgid(void);
u_int64_t queue_generate_evpid(u_int32_t msgid);
struct queue_backend *queue_backend_lookup(const char *);
int queue_message_incoming_path(u_int32_t, char *, size_t);
int queue_envelope_incoming_path(u_int64_t, char *, size_t);
int queue_message_incoming_delete(u_int32_t);
int queue_message_create(u_int32_t *);
int queue_message_delete(u_int32_t);
int queue_message_commit(u_int32_t);
int queue_message_fd_r(u_int32_t);
int queue_message_fd_rw(u_int32_t);
int queue_message_corrupt(u_int32_t);
int queue_envelope_create(struct envelope *);
int queue_envelope_delete(struct envelope *);
int queue_envelope_load(u_int64_t, struct envelope *);
int queue_envelope_update(struct envelope *);
void *qwalk_new(u_int32_t);
int   qwalk(void *, u_int64_t *);
void  qwalk_close(void *);


/* scheduler.c */
pid_t scheduler(void);
void message_reset_flags(struct envelope *);


/* scheduler.c */
struct scheduler_backend *scheduler_backend_lookup(const char *);
void scheduler_info(struct scheduler_info *, struct envelope *);


/* smtp.c */
pid_t smtp(void);
void smtp_resume(void);


/* smtp_session.c */
void session_init(struct listener *, struct session *);
int session_cmp(struct session *, struct session *);
void session_io(struct io *, int);
void session_pickup(struct session *, struct submit_status *);
void session_destroy(struct session *, const char *);
void session_respond(struct session *, char *, ...)
	__attribute__ ((format (printf, 2, 3)));

SPLAY_PROTOTYPE(sessiontree, session, s_nodes, session_cmp);


/* smtpd.c */
int	 child_cmp(struct child *, struct child *);
void imsg_event_add(struct imsgev *);
void imsg_compose_event(struct imsgev *, u_int16_t, u_int32_t, pid_t,
    int, void *, u_int16_t);
void imsg_dispatch(int, short, void *);
const char * proc_to_str(int);
const char * imsg_to_str(int);
SPLAY_PROTOTYPE(childtree, child, entry, child_cmp);


/* ssl.c */
void ssl_init(void);
void ssl_transaction(struct session *);
void ssl_session_init(struct session *);
void ssl_session_destroy(struct session *);
int ssl_load_certfile(const char *, u_int8_t);
void ssl_setup(struct listener *);
int ssl_cmp(struct ssl *, struct ssl *);
void *ssl_mta_init(struct ssl *);
SPLAY_PROTOTYPE(ssltree, ssl, ssl_nodes, ssl_cmp);


/* ssl_privsep.c */
int	 ssl_ctx_use_private_key(void *, char *, off_t);
int	 ssl_ctx_use_certificate_chain(void *, char *, off_t);

/* stats.c */
void	stat_init(struct stat_counter *, int);
size_t	stat_get(int, int);
size_t	stat_increment(int);
size_t	stat_decrement(int);


/* user.c */
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
int mkdir_p(char *, mode_t);
int safe_fclose(FILE *);
int hostname_match(char *, char *);
int email_to_mailaddr(struct mailaddr *, char *);
int valid_localpart(const char *);
int valid_domainpart(const char *);
char *ss_to_text(struct sockaddr_storage *);
char *time_to_text(time_t);
int secure_file(int, char *, char *, uid_t, int);
void lowercase(char *, char *, size_t);
void sa_set_port(struct sockaddr *, int);
u_int64_t generate_uid(void);
void fdlimit(double);
int availdesc(void);
u_int32_t evpid_to_msgid(u_int64_t);
u_int64_t msgid_to_evpid(u_int32_t);
void log_imsg(int, int, struct imsg*);
int ckdir(const char *, mode_t, uid_t, gid_t, int);
int rmtree(char *, int);
int mvpurge(char *, char *);
const char *parse_smtp_response(char *, size_t, char **, int *);
int text_to_netaddr(struct netaddr *, char *);
int text_to_relayhost(struct relayhost *, char *);
