/*	$OpenBSD: smtpd.h,v 1.124 2009/06/03 22:04:15 jacekm Exp $	*/

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

#define CONF_FILE		 "/etc/mail/smtpd.conf"
#define MAX_LISTEN		 16
#define PROC_COUNT		 9
#define READ_BUF_SIZE		 32768
#define MAX_NAME_SIZE		 64

#define MAX_HOPS_COUNT		 100

/* sizes include the tailing '\0' */
#define MAX_LINE_SIZE		 1024
#define MAX_LOCALPART_SIZE	 65
#define MAX_DOMAINPART_SIZE	 MAXHOSTNAMELEN
#define MAX_ID_SIZE		 64

/* return and forward path size */
#define MAX_PATH_SIZE		 256

/*#define SMTPD_CONNECT_TIMEOUT	 (60)*/
#define SMTPD_CONNECT_TIMEOUT	 (10)
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
#define PATH_RUNQUEUEHIGH	"/runqueue-high"
#define PATH_RUNQUEUELOW	"/runqueue-low"

#define PATH_OFFLINE		"/offline"

/* number of MX records to lookup */
#define MAX_MX_COUNT		10

/* max response delay under flood conditions */
#define MAX_RESPONSE_DELAY	60

/* how many responses per state are undelayed */
#define FAST_RESPONSES		2

/* rfc5321 limits */
#define	SMTP_TEXTLINE_MAX	1000
#define	SMTP_CMDLINE_MAX	512
#define	SMTP_ANYLINE_MAX	SMTP_TEXTLINE_MAX

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

struct mxhost {
	TAILQ_ENTRY(mxhost)	 entry;
	struct sockaddr_storage ss;
};

/* buffer specific headers */
struct buf {
	TAILQ_ENTRY(buf)	 entry;
	u_char			*buf;
	size_t			 size;
	size_t			 max;
	size_t			 wpos;
	size_t			 rpos;
	int			 fd;
};

struct msgbuf {
	TAILQ_HEAD(, buf)	 bufs;
	u_int32_t		 queued;
	int			 fd;
};

struct buf_read {
	u_char			 buf[READ_BUF_SIZE];
	u_char			*rptr;
	size_t			 wpos;
};

struct imsg_fd  {
	TAILQ_ENTRY(imsg_fd)	 entry;
	int			 fd;
	u_int32_t		 id;
};

struct imsgbuf {
	TAILQ_HEAD(, imsg_fd)	 fds;
	struct buf_read		 r;
	struct msgbuf		 w;
	struct event		 ev;
	void			(*handler)(int, short, void *);
	int			 fd;
	pid_t			 pid;
	short			 events;
	void			*data;
	u_int32_t		 id;
};

struct imsg_hdr {
	u_int16_t		 type;
	u_int16_t		 len;
	u_int32_t		 peerid;
	pid_t			 pid;
};

struct imsg {
	struct imsg_hdr		 hdr;
	u_int32_t		 id;
	void			*data;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_OK,		/* answer to smtpctl requests */
	IMSG_CTL_FAIL,
	IMSG_CTL_SHUTDOWN,
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
	IMSG_MDA_MAILBOX_FILE,
	IMSG_MDA_MESSAGE_FILE,
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

	IMSG_BATCH_CREATE,
	IMSG_BATCH_APPEND,
	IMSG_BATCH_CLOSE,

	IMSG_PARENT_ENQUEUE_OFFLINE,
	IMSG_PARENT_FORWARD_OPEN,
	IMSG_PARENT_MAILBOX_OPEN,
	IMSG_PARENT_MESSAGE_OPEN,
	IMSG_PARENT_MAILBOX_RENAME,
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

#define IMSG_HEADER_SIZE	 sizeof(struct imsg_hdr)
#define IMSG_DATA_SIZE(imsg)	((imsg)->hdr.len - IMSG_HEADER_SIZE)
#define	MAX_IMSGSIZE		 16384

#define IMSG_SIZE_CHECK(p) do {						\
	if (IMSG_DATA_SIZE(&imsg) != sizeof(*p))			\
		fatalx("bad length imsg received");			\
} while (0)

enum blockmodes {
	BM_NORMAL,
	BM_NONBLOCK
};

struct ctl_conn {
	TAILQ_ENTRY(ctl_conn)	 entry;
	u_int8_t		 flags;
#define CTL_CONN_NOTIFY		 0x01
	struct imsgbuf		 ibuf;
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
	C_DOM
};

struct cond {
	TAILQ_ENTRY(cond)		 c_entry;
	objid_t				 c_map;
	enum cond_type			 c_type;
	struct map			*c_match;
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
#define IS_MAILBOX(x)	((x) == A_MAILDIR || (x) == A_MBOX || (x) == A_FILENAME)
#define IS_RELAY(x)	((x) == A_RELAY || (x) == A_RELAYVIA)
#define IS_EXT(x)	((x) == A_EXT)

struct rule {
	TAILQ_ENTRY(rule)		 r_entry;
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
};

enum path_flags {
	F_PATH_ALIAS = 0x1,
	F_PATH_VIRTUAL = 0x2,
	F_PATH_EXPANDED = 0x4,
	F_PATH_NOFORWARD = 0x8,
	F_PATH_FORWARDED = 0x10,
	F_PATH_ACCOUNT = 0x20,
	F_PATH_AUTHENTICATED = 0x40,
};

struct path {
	TAILQ_ENTRY(path)		 entry;
	struct rule			 rule;
	enum path_flags			 flags;
	u_int8_t			 forwardcnt;
	char				 user[MAX_LOCALPART_SIZE];
	char				 domain[MAX_DOMAINPART_SIZE];
	char				 pw_name[MAXLOGNAME];
	union path_data {
		char filename[MAXPATHLEN];
		char filter[MAXPATHLEN];
	}				 u;
};

enum alias_type {
	ALIAS_USERNAME,
	ALIAS_FILENAME,
	ALIAS_FILTER,
	ALIAS_INCLUDE,
	ALIAS_ADDRESS
};

struct alias {
	TAILQ_ENTRY(alias)		entry;
	enum alias_type			 type;
	union alias_data {
		char username[MAXLOGNAME];
		char filename[MAXPATHLEN];
		char filter[MAXPATHLEN];
		struct path path;
	}                                   u;
};
TAILQ_HEAD(aliaseslist, alias);

enum message_type {
	T_MDA_MESSAGE		= 0x1,
	T_MTA_MESSAGE		= 0x2,
	T_DAEMON_MESSAGE	= 0x4
};

enum message_status {
	S_MESSAGE_LOCKFAILURE	= 0x1,
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
	F_MESSAGE_FORCESCHEDULE	= 0x20
};

struct message {
	TAILQ_ENTRY(message)		 entry;

	enum message_type		 type;

	u_int64_t			 id;
	u_int64_t			 session_id;
	u_int64_t			 batch_id;

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

enum batch_status {
	S_BATCH_PERMFAILURE	= 0x1,
	S_BATCH_TEMPFAILURE	= 0x2,
	S_BATCH_REJECTED	= 0x4,
	S_BATCH_ACCEPTED	= 0x8,
	S_BATCH_RETRY		= 0x10,
	S_BATCH_EDNS		= 0x20,
	S_BATCH_ECONNECT	= 0x40
};

enum batch_type {
	T_MDA_BATCH		= 0x1,
	T_MTA_BATCH		= 0x2,
	T_DAEMON_BATCH		= 0x4
};

enum child_type {
	CHILD_INVALID,
	CHILD_DAEMON,
	CHILD_MDA,
	CHILD_ENQUEUE_OFFLINE
};

struct child {
	SPLAY_ENTRY(child)	entry;

	pid_t			pid;
	enum child_type		type;
	enum smtp_proc_type	title;
};

struct batch {
	SPLAY_ENTRY(batch)	 b_nodes;

	u_int64_t		 id;
	enum batch_type		 type;
	struct rule		 rule;

	struct smtpd		*env;

	char			 message_id[MAX_ID_SIZE];
	char			 hostname[MAXHOSTNAMELEN];
	char			 errorline[MAX_LINE_SIZE];

	struct session		*sessionp;

	struct message		 message;
	struct message		*messagep;
	FILE			*messagefp;
	TAILQ_HEAD(, message)	 messages;

	enum batch_status	 status;
};

enum session_state {
	S_INIT = 0,
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

	char				 credentials[MAX_LINE_SIZE];

	struct batch			*batch;
	TAILQ_HEAD(mxhostlist, mxhost) mxhosts;

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
	struct imsgbuf				*sc_ibufs[PROC_COUNT];
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

struct stats {
	struct s_parent		 parent;
	struct s_queue		 queue;
	struct s_runner		 runner;
	struct s_session	 mta;
	struct s_session	 smtp;
};

struct sched {
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

enum lkasession_flags {
	F_ERROR		= 0x1
};

struct lkasession {
	SPLAY_ENTRY(lkasession)		 nodes;
	u_int64_t			 id;

	struct path			 path;
	struct aliaseslist		 aliaseslist;
	u_int8_t			 iterations;
	u_int32_t			 pending;
	enum lkasession_flags		 flags;
	struct message			 message;
	struct submit_status		 ss;
};

/* aliases.c */
int aliases_exist(struct smtpd *, char *);
int aliases_get(struct smtpd *, struct aliaseslist *, char *);
int aliases_virtual_exist(struct smtpd *, struct path *);
int aliases_virtual_get(struct smtpd *, struct aliaseslist *, struct path *);
int alias_parse(struct alias *, char *);


/* log.c */
void		log_init(int);
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


/* buffer.c */
struct buf	*buf_open(size_t);
struct buf	*buf_dynamic(size_t, size_t);
int		 buf_add(struct buf *, void *, size_t);
void		*buf_reserve(struct buf *, size_t);
int		 buf_close(struct msgbuf *, struct buf *);
void		 buf_free(struct buf *);
void		 msgbuf_init(struct msgbuf *);
void		 msgbuf_clear(struct msgbuf *);
int		 msgbuf_write(struct msgbuf *);


/* dns.c */
void		 dns_query_a(struct smtpd *, char *, int, u_int64_t);
void		 dns_query_mx(struct smtpd *, char *, int, u_int64_t);
void		 dns_query_ptr(struct smtpd *, struct sockaddr_storage *,
		     u_int64_t);
void		 dns_async(struct smtpd *, struct imsgbuf *, int,
		     struct dns *);


/* forward.c */
int forwards_get(int, struct aliaseslist *);


/* imsg.c */
void	 imsg_init(struct imsgbuf *, int, void (*)(int, short, void *));
ssize_t	 imsg_read(struct imsgbuf *);
ssize_t	 imsg_get(struct imsgbuf *, struct imsg *);
int	 imsg_compose(struct imsgbuf *, enum imsg_type, u_int32_t, pid_t,
	    int, void *, u_int16_t);
int	 imsg_composev(struct imsgbuf *, enum imsg_type, u_int32_t,
	    pid_t, int, const struct iovec *, int);
int	 imsg_compose_fds(struct imsgbuf *, enum imsg_type, u_int32_t, pid_t,
	    void *, u_int16_t, int, ...);
struct buf *imsg_create(struct imsgbuf *, enum imsg_type, u_int32_t, pid_t,
	    u_int16_t);
int	 imsg_add(struct buf *, void *, u_int16_t);
int	 imsg_append(struct imsgbuf *, struct buf *);
int	 imsg_close(struct imsgbuf *, struct buf *);
void	 imsg_free(struct imsg *);
void	 imsg_event_add(struct imsgbuf *); /* needs to be provided externally */
int	 imsg_get_fd(struct imsgbuf *, struct imsg *);
int	 imsg_flush(struct imsgbuf *);
void	 imsg_clear(struct imsgbuf *);

/* smtpd.c */
int	 child_cmp(struct child *, struct child *);
SPLAY_PROTOTYPE(childtree, child, entry, child_cmp);

/* lka.c */
pid_t		 lka(struct smtpd *);
int		 lkasession_cmp(struct lkasession *, struct lkasession *);
SPLAY_PROTOTYPE(lkatree, lkasession, nodes, lkasession_cmp);

/* mfa.c */
pid_t		 mfa(struct smtpd *);
int		 msg_cmp(struct message *, struct message *);

/* queue.c */
pid_t		 queue(struct smtpd *);
u_int64_t	 queue_generate_id(void);
int		 queue_remove_batch_message(struct smtpd *, struct batch *,
 		     struct message *);
int		 queue_load_envelope(struct message *, char *);
int		 queue_update_envelope(struct message *);
int		 queue_remove_envelope(struct message *);
int		 batch_cmp(struct batch *, struct batch *);
struct batch    *batch_by_id(struct smtpd *, u_int64_t);
struct message	*message_by_id(struct smtpd *, struct batch *, u_int64_t);
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
char		*map_dblookup(struct smtpd *, char *, char *);

/* mda.c */
pid_t		 mda(struct smtpd *);

/* mta.c */
pid_t		 mta(struct smtpd *);

/* control.c */
pid_t		 control(struct smtpd *);
void		 session_socket_blockmode(int, enum blockmodes);

/* enqueue.c */
int		 enqueue(int, char **);
int		 enqueue_offline(int, char **);

/* runner.c */
pid_t		 runner(struct smtpd *);
SPLAY_PROTOTYPE(batchtree, batch, b_nodes, batch_cmp);


/* smtp.c */
pid_t		 smtp(struct smtpd *);

/* smtp_session.c */
void		 session_init(struct listener *, struct session *);
int		 session_cmp(struct session *, struct session *);
void		 session_pickup(struct session *, struct submit_status *);
void		 session_destroy(struct session *);
void		 session_respond(struct session *, char *, ...)
		    __attribute__ ((format (printf, 2, 3)));
void		 session_bufferevent_new(struct session *);

SPLAY_PROTOTYPE(sessiontree, session, s_nodes, session_cmp);

/* store.c */
int store_write_header(struct batch *, struct message *, FILE *, int);
int store_write_message(struct batch *, struct message *);
int store_write_daemon(struct batch *, struct message *);
int store_message(struct batch *, struct message *,
    int (*)(struct batch *, struct message *));

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
int		 secure_file(int, char *, struct passwd *);
void		 lowercase(char *, char *, size_t);
