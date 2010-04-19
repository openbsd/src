/*	$OpenBSD: smtpd.h,v 1.172 2010/04/19 08:14:07 jacekm Exp $	*/

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

#include			 <imsg.h>

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

#define IMSG_SIZE_CHECK(p) do {					\
	if (IMSG_DATA_SIZE(&imsg) != sizeof(*p))		\
		fatalx("bad length imsg received");		\
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
#define MAX_ID_SIZE		 64
#define MAX_TAG_SIZE		 32

/* return and forward path size */
#define MAX_PATH_SIZE		 256

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

#define PATH_RUNQUEUE		"/runqueue"

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

struct netaddr {
	struct sockaddr_storage ss;
	int bits;
};

struct relayhost {
	u_int8_t flags;
	char hostname[MAXHOSTNAMELEN];
	u_int16_t port;
	char cert[PATH_MAX];
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_OK,		/* answer to smtpctl requests */
	IMSG_CTL_FAIL,
	IMSG_CTL_SHUTDOWN,
	IMSG_CTL_VERBOSE,
	IMSG_CONF_START,
	IMSG_CONF_SSL,
	IMSG_CONF_SSL_CERT,
	IMSG_CONF_SSL_KEY,
	IMSG_CONF_LISTENER,
	IMSG_CONF_MAP,
	IMSG_CONF_MAP_CONTENT,
	IMSG_CONF_RULE,
	IMSG_CONF_RULE_SOURCE,
	IMSG_CONF_CONDITION,
	IMSG_CONF_OPTION,
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
	IMSG_QUEUE_STATS,

	IMSG_QUEUE_REMOVE_SUBMISSION,
	IMSG_QUEUE_MESSAGE_UPDATE,
	IMSG_QUEUE_MESSAGE_FD,
	IMSG_QUEUE_MESSAGE_FILE,

	IMSG_RUNNER_UPDATE_ENVELOPE,
	IMSG_RUNNER_STATS,
	IMSG_RUNNER_SCHEDULE,
	IMSG_RUNNER_REMOVE,

	IMSG_BATCH_CREATE,
	IMSG_BATCH_APPEND,
	IMSG_BATCH_CLOSE,
	IMSG_BATCH_DONE,

	IMSG_PARENT_ENQUEUE_OFFLINE,
	IMSG_PARENT_FORWARD_OPEN,
	IMSG_PARENT_FORK_MDA,
	IMSG_PARENT_STATS,

	IMSG_PARENT_AUTHENTICATE,
	IMSG_PARENT_SEND_CONFIG,

	IMSG_MDA_PAUSE,
	IMSG_MTA_PAUSE,
	IMSG_SMTP_PAUSE,
	IMSG_SMTP_STATS,

	IMSG_MDA_RESUME,
	IMSG_MTA_RESUME,
	IMSG_SMTP_RESUME,

	IMSG_STATS,

	IMSG_SMTP_ENQUEUE,

	IMSG_DNS_A,
	IMSG_DNS_A_END,
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
	short			 events;
};

struct ctl_conn {
	TAILQ_ENTRY(ctl_conn)	 entry;
	u_int8_t		 flags;
#define CTL_CONN_NOTIFY		 0x01
	struct imsgev		 iev;
};
TAILQ_HEAD(ctl_connlist, ctl_conn);

typedef u_int32_t		 objid_t;

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
	S_FILE,
	S_DB,
	S_EXT
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

enum opt_type {
	O_RWUSER,			/* rewrite user */
	O_RWDOMAIN,			/* rewrite domain */
};

struct opt {
	TAILQ_ENTRY(opt)		 o_entry;
	enum opt_type			 o_type;
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

#define IS_MAILBOX(x)	((x).rule.r_action == A_MAILDIR || (x).rule.r_action == A_MBOX || (x).rule.r_action == A_FILENAME)
#define IS_RELAY(x)	((x).rule.r_action == A_RELAY || (x).rule.r_action == A_RELAYVIA)
#define IS_EXT(x)	((x).rule.r_action == A_EXT)

struct rule {
	TAILQ_ENTRY(rule)		 r_entry;
	char				 r_tag[MAX_TAG_SIZE];
	int				 r_accept;
	struct map			*r_sources;
	TAILQ_HEAD(condlist, cond)	 r_conditions;
	enum action_type		 r_action;
	union rule_dest {
		char			 path[MAXPATHLEN];
		struct relayhost       	 relayhost;
#define	MAXCOMMANDLEN	256
		char			 command[MAXCOMMANDLEN];
	}				 r_value;
	TAILQ_HEAD(optlist, opt)	 r_options;

	char				*r_user;
	objid_t				 r_amap;
};

enum path_flags {
	F_PATH_ALIAS = 0x1,
	F_PATH_VIRTUAL = 0x2,
	F_PATH_EXPANDED = 0x4,
	F_PATH_NOFORWARD = 0x8,
	F_PATH_FORWARDED = 0x10,
	F_PATH_ACCOUNT = 0x20,
	F_PATH_AUTHENTICATED = 0x40,
	F_PATH_RELAY = 0x80,
};

struct mailaddr {
	char	user[MAX_LOCALPART_SIZE];
	char	domain[MAX_DOMAINPART_SIZE];
};

union path_data {
	char username[MAXLOGNAME];
	char filename[MAXPATHLEN];
	char filter[MAXPATHLEN];
	struct mailaddr mailaddr;
};

struct path {
	TAILQ_ENTRY(path)		 entry;
	struct rule			 rule;
	struct cond			*cond;
	enum path_flags			 flags;
	u_int8_t			 forwardcnt;
	char				 user[MAX_LOCALPART_SIZE];
	char				 domain[MAX_DOMAINPART_SIZE];
	char				 pw_name[MAXLOGNAME];
	union path_data			 u;
};
TAILQ_HEAD(deliverylist, path);

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

struct expand_node {
	RB_ENTRY(expand_node)	entry;
	size_t			refcnt;
	enum expand_flags      	flags;
	enum expand_type       	type;
	union path_data		u;
};

struct alias {
	enum expand_type type;
	union path_data		u;
};

enum message_type {
	T_MDA_MESSAGE		= 0x1,
	T_MTA_MESSAGE		= 0x2,
	T_BOUNCE_MESSAGE	= 0x4
};

enum message_status {
	S_MESSAGE_PERMFAILURE	= 0x2,
	S_MESSAGE_TEMPFAILURE	= 0x4,
	S_MESSAGE_REJECTED	= 0x8,
	S_MESSAGE_ACCEPTED	= 0x10,
	S_MESSAGE_RETRY		= 0x20,
	S_MESSAGE_EDNS		= 0x40,
	S_MESSAGE_ECONNECT	= 0x80
};

enum message_flags {
	F_MESSAGE_RESOLVED	= 0x1,
	F_MESSAGE_SCHEDULED	= 0x2,
	F_MESSAGE_PROCESSING	= 0x4,
	F_MESSAGE_AUTHENTICATED	= 0x8,
	F_MESSAGE_ENQUEUED	= 0x10,
	F_MESSAGE_FORCESCHEDULE	= 0x20,
	F_MESSAGE_BOUNCE	= 0x40
};

struct message {
	TAILQ_ENTRY(message)		 entry;

	enum message_type		 type;

	u_int64_t			 id;
	u_int64_t			 session_id;
	u_int64_t			 batch_id;

	char				 tag[MAX_TAG_SIZE];

	char				 message_id[MAX_ID_SIZE];
	char				 message_uid[MAX_ID_SIZE];

	char				 session_helo[MAXHOSTNAMELEN];
	char				 session_hostname[MAXHOSTNAMELEN];
	char				 session_errorline[MAX_LINE_SIZE];
	struct sockaddr_storage		 session_ss;
	struct path			 session_rcpt;

	struct path			 sender;
	struct path			 recipient;

	time_t				 creation;
	time_t				 lasttry;
	u_int8_t			 retry;
	enum message_flags		 flags;
	enum message_status		 status;
};

enum batch_type {
	T_MDA_BATCH		= 0x1,
	T_MTA_BATCH		= 0x2,
	T_BOUNCE_BATCH		= 0x4
};

struct batch {
	SPLAY_ENTRY(batch)	 b_nodes;
	u_int64_t		 id;
	enum batch_type		 type;
	struct rule		 rule;
	struct smtpd		*env;
	char			 message_id[MAX_ID_SIZE];
	char			 hostname[MAXHOSTNAMELEN];
	TAILQ_HEAD(, message)	 messages;
};

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
	u_int8_t		 flags;
};

struct listener {
	u_int8_t		 flags;
	int			 fd;
	struct sockaddr_storage	 ss;
	in_port_t		 port;
	struct timeval		 timeout;
	struct event		 ev;
	struct smtpd		*env;
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
	struct smtpd			*s_env;
	void				*s_ssl;
	u_char				*s_buf;
	int				 s_buflen;
	struct timeval			 s_tv;
	struct message			 s_msg;
	short				 s_nresp[STATE_COUNT];
	size_t				 rcptcount;
	long				 s_datalen;

	struct auth			 s_auth;
	struct batch			*batch;

	FILE				*datafp;
	int				 mboxfd;
	int				 messagefd;
};

struct smtpd {
	char					 sc_conffile[MAXPATHLEN];

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
	TAILQ_HEAD(listenerlist, listener)	*sc_listeners;
	TAILQ_HEAD(maplist, map)		*sc_maps, *sc_maps_reload;
	TAILQ_HEAD(rulelist, rule)		*sc_rules, *sc_rules_reload;
	SPLAY_HEAD(sessiontree, session)	 sc_sessions;
	SPLAY_HEAD(msgtree, message)		 sc_messages;
	SPLAY_HEAD(ssltree, ssl)		*sc_ssl;

	SPLAY_HEAD(batchtree, batch)		 batch_queue;
	SPLAY_HEAD(childtree, child)		 children;
	SPLAY_HEAD(lkatree, lkasession)		 lka_sessions;
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
	size_t		bounces_active;
	size_t		bounces;
};

struct s_session {
	size_t		sessions;
	size_t		sessions_active;

	size_t		smtps;
	size_t		smtps_active;

	size_t		starttls;
	size_t		starttls_active;

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
};

struct s_control {
	size_t		sessions;
	size_t		sessions_active;
};

struct stats {
	struct s_parent		 parent;
	struct s_queue		 queue;
	struct s_runner		 runner;
	struct s_session	 mta;
	struct s_mda		 mda;
	struct s_session	 smtp;
	struct s_control	 control;
};

struct sched {
	int			fd;
	char			mid[MAX_ID_SIZE];
	int			ret;
};

struct remove {
	int			fd;
	char			mid[MAX_ID_SIZE];
	int			ret;
};

struct reload {
	int			fd;
	int			ret;
};

struct submit_status {
	u_int64_t			 id;
	int				 code;
	union submit_path {
		struct path		 path;
		char			 msgid[MAX_ID_SIZE];
		char			 errormsg[MAX_LINE_SIZE];
	}				 u;
	enum message_flags		 flags;
	struct sockaddr_storage		 ss;
	struct message			 msg;
};

struct forward_req {
	u_int64_t			 id;
	u_int8_t			 status;
	char				 pw_name[MAXLOGNAME];
};

struct dns {
	u_int64_t		 id;
	char			 host[MAXHOSTNAMELEN];
	int			 port;
	int			 error;
	struct sockaddr_storage	 ss;
	struct smtpd		*env;
	struct dns		*next;
};

struct secret {
	u_int64_t		 id;
	char			 host[MAXHOSTNAMELEN];
	char			 secret[MAX_LINE_SIZE];
};

struct mda_session {
	LIST_ENTRY(mda_session)	 entry;
	struct message		 msg;
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

enum lkasession_flags {
	F_ERROR		= 0x1
};

struct lkasession {
	SPLAY_ENTRY(lkasession)		 nodes;
	u_int64_t			 id;

	struct path			 path;
	struct deliverylist    		 deliverylist;

	RB_HEAD(expandtree, expand_node)	expandtree;

	u_int8_t			 iterations;
	u_int32_t			 pending;
	enum lkasession_flags		 flags;
	struct message			 message;
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
#define	MTA_FORCE_ANYSSL	0x1
#define	MTA_FORCE_SMTPS		0x2
#define	MTA_ALLOW_PLAIN		0x4
#define	MTA_USE_AUTH		0x8

struct mta_relay {
	TAILQ_ENTRY(mta_relay)	 entry;
	struct sockaddr_storage	 sa;
	char			 fqdn[MAXHOSTNAMELEN];
	int			 used;
};

struct mta_session {
	SPLAY_ENTRY(mta_session) entry;
	u_int64_t		 id;
	struct smtpd		*env;
	enum mta_state		 state;
	char			*host;
	int			 port;
	int			 flags;
	TAILQ_HEAD(,message)	 recipients;
	TAILQ_HEAD(,mta_relay)	 relays;
	char			*secret;
	int			 fd;
	int			 datafd;
	struct event		 ev;
	char			*cert;
	void			*pcb;
};

/* aliases.c */
int aliases_exist(struct smtpd *, objid_t, char *);
int aliases_get(struct smtpd *, objid_t, struct expandtree *, char *);
int aliases_vdomain_exists(struct smtpd *, objid_t, char *);
int aliases_virtual_exist(struct smtpd *, objid_t, struct path *);
int aliases_virtual_get(struct smtpd *, objid_t, struct expandtree *, struct path *);
int alias_parse(struct alias *, char *);
void alias_to_expand_node(struct expand_node *, struct alias *);

/* authenticate.c */
int authenticate_user(char *, char *);

/* bounce.c */
void bounce_process(struct smtpd *, struct message *);
int bounce_session(struct smtpd *, int, struct message *);
int bounce_session_switch(struct smtpd *, FILE *, enum session_state *, char *,
	struct message *);

/* log.c */
void		log_init(int);
void		log_verbose(int);
void		log_warn(const char *, ...)
    __attribute__ ((format (printf, 1, 2)));
void		log_warnx(const char *, ...)
    __attribute__ ((format (printf, 1, 2)));
void		log_info(const char *, ...)
    __attribute__ ((format (printf, 1, 2)));
void		log_debug(const char *, ...)
    __attribute__ ((format (printf, 1, 2)));
__dead void	fatal(const char *);
__dead void	fatalx(const char *);


/* dns.c */
void		 dns_query_a(struct smtpd *, char *, int, u_int64_t);
void		 dns_query_mx(struct smtpd *, char *, int, u_int64_t);
void		 dns_query_ptr(struct smtpd *, struct sockaddr_storage *,
		     u_int64_t);
void		 dns_async(struct smtpd *, struct imsgev *, int,
		     struct dns *);
/* expand.c */
int expand_cmp(struct expand_node *, struct expand_node *);
void expandtree_increment_node(struct expandtree *, struct expand_node *);
void expandtree_decrement_node(struct expandtree *, struct expand_node *);
void expandtree_remove_node(struct expandtree *, struct expand_node *);
struct expand_node *expandtree_lookup(struct expandtree *, struct expand_node *);
RB_PROTOTYPE(expandtree, expand_node, nodes, expand_cmp);

/* forward.c */
int forwards_get(int, struct expandtree *);

/* smtpd.c */
int	 child_cmp(struct child *, struct child *);
SPLAY_PROTOTYPE(childtree, child, entry, child_cmp);
void	 imsg_event_add(struct imsgev *);
int	 imsg_compose_event(struct imsgev *, u_int16_t, u_int32_t, pid_t,
	    int, void *, u_int16_t);

/* lka.c */
pid_t		 lka(struct smtpd *);
int		 lkasession_cmp(struct lkasession *, struct lkasession *);
SPLAY_PROTOTYPE(lkatree, lkasession, nodes, lkasession_cmp);

/* mfa.c */
pid_t		 mfa(struct smtpd *);
int		 msg_cmp(struct message *, struct message *);

/* queue.c */
pid_t		 queue(struct smtpd *);
int		 queue_load_envelope(struct message *, char *);
int		 queue_update_envelope(struct message *);
int		 queue_remove_envelope(struct message *);
void		 queue_submit_envelope(struct smtpd *, struct message *);
void		 queue_commit_envelopes(struct smtpd *, struct message*);
int		 batch_cmp(struct batch *, struct batch *);
struct batch    *batch_by_id(struct smtpd *, u_int64_t);
u_int16_t	 queue_hash(char *);

/* queue_shared.c */
int		 queue_create_layout_message(char *, char *);
void		 queue_delete_layout_message(char *, char *);
int		 queue_record_layout_envelope(char *, struct message *);
int		 queue_remove_layout_envelope(char *, struct message *);
int		 queue_commit_layout_message(char *, struct message *);
int		 queue_open_layout_messagefile(char *, struct message *);
int		 enqueue_create_layout(char *);
void		 enqueue_delete_message(char *);
int		 enqueue_record_envelope(struct message *);
int		 enqueue_remove_envelope(struct message *);
int		 enqueue_commit_message(struct message *);
int		 enqueue_open_messagefile(struct message *);
int		 bounce_create_layout(char *, struct message *);
void		 bounce_delete_message(char *);
int		 bounce_record_envelope(struct message *);
int		 bounce_remove_envelope(struct message *);
int		 bounce_commit_message(struct message *);
int		 bounce_record_message(struct message *);
int		 queue_create_incoming_layout(char *);
void		 queue_delete_incoming_message(char *);
int		 queue_record_incoming_envelope(struct message *);
int		 queue_remove_incoming_envelope(struct message *);
int		 queue_commit_incoming_message(struct message *);
int		 queue_open_incoming_message_file(struct message *);
int		 queue_open_message_file(char *msgid);
void		 queue_message_update(struct message *);
void		 queue_delete_message(char *);
struct qwalk	*qwalk_new(char *);
int		 qwalk(struct qwalk *, char *);
void		 qwalk_close(struct qwalk *);
void		 show_queue(char *, int);

u_int16_t	queue_hash(char *);

/* map.c */
char		*map_lookup(struct smtpd *, objid_t, char *);

/* mda.c */
pid_t		 mda(struct smtpd *);

/* mta.c */
pid_t		 mta(struct smtpd *);
int		 mta_session_cmp(struct mta_session *, struct mta_session *);
SPLAY_PROTOTYPE(mtatree, mta_session, entry, mta_session_cmp);

/* control.c */
pid_t		 control(struct smtpd *);
void		 session_socket_blockmode(int, enum blockmodes);
void		 session_socket_no_linger(int);
int		 session_socket_error(int);

/* enqueue.c */
int		 enqueue(int, char **);
int		 enqueue_offline(int, char **);

/* runner.c */
pid_t		 runner(struct smtpd *);
void		 message_reset_flags(struct message *);
SPLAY_PROTOTYPE(batchtree, batch, b_nodes, batch_cmp);

/* smtp.c */
pid_t		 smtp(struct smtpd *);
void		 smtp_resume(struct smtpd *);

/* smtp_session.c */
void		 session_init(struct listener *, struct session *);
int		 session_cmp(struct session *, struct session *);
void		 session_pickup(struct session *, struct submit_status *);
void		 session_destroy(struct session *);
void		 session_respond(struct session *, char *, ...)
		    __attribute__ ((format (printf, 2, 3)));
void		 session_bufferevent_new(struct session *);

SPLAY_PROTOTYPE(sessiontree, session, s_nodes, session_cmp);

/* config.c */
#define		 PURGE_LISTENERS	0x01
#define		 PURGE_MAPS		0x02
#define		 PURGE_RULES		0x04
#define		 PURGE_SSL		0x08
#define		 PURGE_EVERYTHING	0xff
void		 purge_config(struct smtpd *, u_int8_t);
void		 unconfigure(struct smtpd *);
void		 configure(struct smtpd *);
void		 init_pipes(struct smtpd *);
void		 config_pipes(struct smtpd *, struct peer *, u_int);
void		 config_peers(struct smtpd *, struct peer *, u_int);

/* parse.y */
int		 parse_config(struct smtpd *, const char *, int);
int		 cmdline_symset(char *);

/* ssl.c */
void	 ssl_init(void);
void	 ssl_transaction(struct session *);

void	 ssl_session_init(struct session *);
void	 ssl_session_destroy(struct session *);
int	 ssl_load_certfile(struct smtpd *, const char *, u_int8_t);
void	 ssl_setup(struct smtpd *, struct listener *);
int	 ssl_cmp(struct ssl *, struct ssl *);
SPLAY_PROTOTYPE(ssltree, ssl, ssl_nodes, ssl_cmp);

/* ssl_privsep.c */
int	 ssl_ctx_use_private_key(void *, char *, off_t);
int	 ssl_ctx_use_certificate_chain(void *, char *, off_t);

/* map.c */
struct map	*map_find(struct smtpd *, objid_t);
struct map	*map_findbyname(struct smtpd *, const char *);

/* util.c */
typedef struct arglist arglist;
struct arglist {
	char    **list;
	u_int   num;
	u_int   nalloc;
};
void		 addargs(arglist *, char *, ...)
		     __attribute__((format(printf, 2, 3)));
int		 bsnprintf(char *, size_t, const char *, ...)
    __attribute__ ((format (printf, 3, 4)));
int		 safe_fclose(FILE *);
int		 hostname_match(char *, char *);
int		 recipient_to_path(struct path *, char *);
int		 valid_localpart(char *);
int		 valid_domainpart(char *);
char		*ss_to_text(struct sockaddr_storage *);
int		 valid_message_id(char *);
int		 valid_message_uid(char *);
char		*time_to_text(time_t);
int		 secure_file(int, char *, struct passwd *, int);
void		 lowercase(char *, char *, size_t);
void		 message_set_errormsg(struct message *, char *, ...);
char		*message_get_errormsg(struct message *);
void		 sa_set_port(struct sockaddr *, int);
struct path	*path_dup(struct path *);
u_int64_t	 generate_uid(void);
void		 fdlimit(double);
int		 availdesc(void);
