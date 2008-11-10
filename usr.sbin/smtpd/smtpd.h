/*	$OpenBSD: smtpd.h,v 1.6 2008/11/10 21:29:18 chl Exp $	*/

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
#define STRLEN			 1024
#define PROC_COUNT		 8
#define READ_BUF_SIZE		 32768
#define MAX_NAME_SIZE		 64

/* sizes include the tailing '\0' */
#define MAX_LOCALPART_SIZE	 65
#define MAX_DOMAINPART_SIZE	 MAXHOSTNAMELEN

/* return and forward path size */
#define MAX_PATH_SIZE		 256

/*#define SMTPD_CONNECT_TIMEOUT	 (60)*/
#define SMTPD_CONNECT_TIMEOUT	 (10)
#define SMTPD_QUEUE_INTERVAL	 (15 * 60)
#define SMTPD_QUEUE_MAXINTERVAL	 (4 * 60 * 60)
#define SMTPD_QUEUE_EXPIRY	 (4 * 24 * 60 * 60)
#define SMTPD_USER		 "_smtpd"
#define SMTPD_SOCKET		 "/var/run/smtpd.sock"
#define SMTPD_BANNER		 "220 %s OpenSMTPD\r\n"
#define SMTPD_SESSION_TIMEOUT	 300

#define RCPTBUFSZ		 256

#define PATH_SPOOL		"/var/spool/smtpd"

#define PATH_MESSAGES		"/messages"
#define PATH_LOCAL		"/local"
#define PATH_RELAY		"/relay"
#define PATH_DAEMON		"/daemon"
#define PATH_ENVELOPES		"/envelopes"

/* used by newaliases */
#define	PATH_ALIASES		"/etc/mail/aliases"
#define	PATH_ALIASESDB		"/etc/mail/aliases.db"

/* number of MX records to lookup */
#define MXARRAYSIZE	5

struct address {
	char hostname[MAXHOSTNAMELEN];
	u_int16_t port;
};

struct netaddr {
	struct sockaddr_storage ss;
	int masked;
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
	IMSG_CONF_RULE,
	IMSG_CONF_CONDITION,
	IMSG_CONF_OPTION,
	IMSG_CONF_END,
	IMSG_CONF_RELOAD,
	IMSG_LKA_LOOKUP_MAIL,
	IMSG_LKA_LOOKUP_RCPT,
	IMSG_LKA_ALIAS_LOOKUP,
	IMSG_LKA_VUSER_LOOKUP,
	IMSG_LKA_ALIAS_RESULT,
	IMSG_LKA_VUSER_RESULT,
	IMSG_LKA_ALIAS_RESULT_ACK,
	IMSG_LKA_ALIAS_SCHEDULE,
	IMSG_LKA_ALIAS_END,
	IMSG_LKA_NO_ALIAS,
	IMSG_LKA_MX_LOOKUP,
	IMSG_LKA_FORWARD_LOOKUP,
	IMSG_LKA_HOSTNAME_LOOKUP,
	IMSG_MDA_MAILBOX_FILE,
	IMSG_MDA_MESSAGE_FILE,
	IMSG_MDA_MAILBOX_FILE_ERROR,
	IMSG_MDA_MESSAGE_FILE_ERROR,
	IMSG_MFA_RPATH_SUBMIT,
	IMSG_MFA_RCPT_SUBMIT,
	IMSG_MFA_DATA_SUBMIT,
	IMSG_MFA_LOOKUP_MAIL,
	IMSG_MFA_LOOKUP_RCPT,
	IMSG_QUEUE_REMOVE_SUBMISSION,
	IMSG_QUEUE_CREATE_MESSAGE_FILE,
	IMSG_QUEUE_DELETE_MESSAGE_FILE,
	IMSG_QUEUE_MESSAGE_SUBMIT,
	IMSG_QUEUE_MESSAGE_UPDATE,
	IMSG_QUEUE_BATCH_COMPLETE,
	IMSG_QUEUE_BATCH_CLOSE,
	IMSG_QUEUE_MESSAGE_FD,

	IMSG_QUEUE_ACCEPTED_CLOSE,
	IMSG_QUEUE_RETRY_CLOSE,
	IMSG_QUEUE_REJECTED_CLOSE,

	IMSG_QUEUE_RECIPIENT_ACCEPTED,
	IMSG_QUEUE_RECIPIENT_UPDATED,

	IMSG_CREATE_BATCH,
	IMSG_BATCH_APPEND,
	IMSG_BATCH_CLOSE,

	IMSG_SMTP_MESSAGE_FILE,
	IMSG_SMTP_SUBMIT_ACK,
	IMSG_SMTP_HOSTNAME_ANSWER,
	IMSG_PARENT_MAILBOX_OPEN,
	IMSG_PARENT_MESSAGE_OPEN,
	IMSG_PARENT_MAILBOX_RENAME,

	IMSG_PARENT_AUTHENTICATE
};

#define IMSG_HEADER_SIZE	 sizeof(struct imsg_hdr)
#define	MAX_IMSGSIZE		 16384

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
		char			 med_string[STRLEN];
		struct netaddr		 med_addr;
	}				 me_key;
	union mapel_data		 me_val;
};

struct map {
	TAILQ_ENTRY(map)		 m_entry;
#define F_USED				 0x01
#define F_DYNAMIC			 0x02
	u_int8_t			 m_flags;
	char				 m_name[STRLEN];
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
	union {
		char			 path[MAXPATHLEN];
		struct address		 host;
#define	MAXCOMMANDLEN	256
		char			 command[MAXCOMMANDLEN];
	}				 r_value;
	TAILQ_HEAD(optlist, opt)	 r_options;
};

enum path_flags {
	F_ALIAS = 0x1,
	F_VIRTUAL = 0x2,
	F_EXPANDED = 0x4,
	F_NOFORWARD = 0x8,
	F_FORWARDED = 0x10
};

struct path {
	TAILQ_ENTRY(path)		 entry;
	struct rule			 rule;
	enum path_flags			 flags;
	u_int8_t			 forwardcnt;
	char				 user[MAX_LOCALPART_SIZE];
	char				 domain[MAX_DOMAINPART_SIZE];
	char				 pw_name[MAXLOGNAME];
	union {
		char filename[MAXPATHLEN];
		char filter[MAXPATHLEN];
	} u;
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
	union {
		char username[MAXLOGNAME];
		char filename[MAXPATHLEN];
		char filter[MAXPATHLEN];
		struct path path;
	} u;
};
TAILQ_HEAD(aliaseslist, alias);

struct submit_status {
	u_int64_t			 id;
	int				 code;
	union {
		struct path		 path;
		char			 msgid[MAXPATHLEN];
		char			 errormsg[STRLEN];
	} u;
	struct sockaddr_storage		 ss;
};

struct message_recipient {
	u_int64_t			 id;
	struct sockaddr_storage		 ss;
	struct path			 path;
};

enum message_type {
	T_MDA_MESSAGE		= 0x1,
	T_MTA_MESSAGE		= 0x2,
	T_DAEMON_MESSAGE	= 0x4
};

enum message_status {
	S_MESSAGE_PERMFAILURE	= 0x1,
	S_MESSAGE_TEMPFAILURE	= 0x2,
	S_MESSAGE_REJECTED	= 0x4,
	S_MESSAGE_ACCEPTED	= 0x8,
	S_MESSAGE_RETRY		= 0x10,
	S_MESSAGE_EDNS		= 0x20,
	S_MESSAGE_ECONNECT	= 0x40
};

enum message_flags {
	F_MESSAGE_COMPLETE	= 0x1,
	F_MESSAGE_RESOLVED	= 0x2,
	F_MESSAGE_READY		= 0x4,
	F_MESSAGE_EXPIRED	= 0x8,
	F_MESSAGE_PROCESSING	= 0x10
};

struct message {
	SPLAY_ENTRY(message)		 nodes;
	TAILQ_ENTRY(message)		 entry;

	enum message_type		 type;

	u_int64_t			 id;
	u_int64_t			 session_id;
	u_int64_t			 batch_id;

	char				 message_id[MAXPATHLEN];
	char				 message_uid[MAXPATHLEN];

	char				 session_helo[MAXHOSTNAMELEN];
	char				 session_hostname[MAXHOSTNAMELEN];
	char				 session_errorline[STRLEN];
	struct sockaddr_storage		 session_ss;

	struct path			 sender;
	struct path			 recipient;
	TAILQ_HEAD(pathlist,path)	 recipients;

	u_int16_t			 rcptcount;

	time_t				 creation;
	time_t				 lasttry;
	u_int8_t			 retry;
	enum message_flags		 flags;
	enum message_status		 status;
	FILE				*datafp;
	int				 mboxfd;
	int				 messagefd;
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

enum batch_flags {
	F_BATCH_COMPLETE	= 0x1,
	F_BATCH_RESOLVED	= 0x2,
	F_BATCH_SCHEDULED	= 0x4,
	F_BATCH_EXPIRED		= 0x8,
};

struct mdaproc {
	SPLAY_ENTRY(mdaproc)	mdaproc_nodes;

	pid_t			pid;
};

struct batch {
	SPLAY_ENTRY(batch)	 b_nodes;

	u_int64_t		 id;
	enum batch_type		 type;
	enum batch_flags	 flags;

	struct rule			 rule;

	struct event			 ev;
	struct timeval			 tv;
	int				 peerfd;
	struct bufferevent		*bev;
	u_int8_t			 state;
	struct smtpd			*env;

	char				 message_id[MAXPATHLEN];
	char				 hostname[MAXHOSTNAMELEN];
	char				 errorline[STRLEN];

	u_int8_t			 getaddrinfo_error;
	struct sockaddr_storage		 ss[MXARRAYSIZE*2];
	u_int8_t			 ss_cnt;
	u_int8_t			 ss_off;

	time_t				 creation;
	time_t				 lasttry;
	u_int8_t			 retry;

	struct message			message;
	struct message			*messagep;
	FILE				*messagefp;
	TAILQ_HEAD(messagelist, message) messages;

	enum batch_status		status;
};

enum session_state {
	S_INIT = 0,
	S_GREETED,
	S_TLS,
	S_AUTH,
	S_HELO,
	S_MAIL,
	S_RCPT,
	S_DATA,
	S_DATACONTENT,
	S_DONE,
	S_QUIT
};

struct ssl {
	SPLAY_ENTRY(ssl)	 ssl_nodes;
	char			 ssl_name[PATH_MAX];
	char			*ssl_cert;
	off_t			 ssl_cert_len;
	char			*ssl_key;
	off_t			 ssl_key_len;
};

struct listener {
#define F_STARTTLS		 0x01
#define F_SSMTP			 0x02
#define F_SSL			(F_SSMTP|F_STARTTLS)
	u_int8_t		 flags;
	int			 fd;
	struct sockaddr_storage	 ss;
	in_port_t		 port;
	int			 backlog;
	struct timeval		 timeout;
	struct event		 ev;
	struct smtpd		*env;
	char			 ssl_cert_name[PATH_MAX];
	struct ssl		*ssl;
	void			*ssl_ctx;
	TAILQ_ENTRY(listener)	 entry;
};

struct session_auth_req {
	u_int64_t	session_id;
	char		buffer[STRLEN];
};

struct session_auth_reply {
	u_int64_t	session_id;
	u_int8_t	value;
};

enum session_flags {
	F_QUIT		= 0x1,
	F_IMSG_SENT	= 0x2,
	F_8BITMIME	= 0x4,
	F_SECURE	= 0x8,
	F_AUTHENTICATED	= 0x10
};

struct session {
	SPLAY_ENTRY(session)		 s_nodes;
	u_int64_t			 s_id;

	enum session_flags		 s_flags;
	enum session_state		 s_state;
	time_t				 s_tm;
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
};

struct smtpd {
#define SMTPD_OPT_VERBOSE			 0x00000001
#define SMTPD_OPT_NOACTION			 0x00000002
	u_int32_t				 sc_opts;
#define SMTPD_CONFIGURING			 0x00000001
#define SMTPD_EXITING				 0x00000002
	u_int32_t				 sc_flags;
	struct timeval				 sc_qintval;
	struct event				 sc_ev;
	int					 sc_pipes[PROC_COUNT]
						    [PROC_COUNT][2];
	struct imsgbuf				*sc_ibufs[PROC_COUNT];
	struct passwd				*sc_pw;
	char					 sc_hostname[MAXHOSTNAMELEN];
	TAILQ_HEAD(listenerlist, listener)	 sc_listeners;
	TAILQ_HEAD(maplist, map)		*sc_maps;
	TAILQ_HEAD(rulelist, rule)		*sc_rules;
	SPLAY_HEAD(sessiontree, session)	 sc_sessions;
	SPLAY_HEAD(msgtree, message)		 sc_messages;
	SPLAY_HEAD(ssltree, ssl)		 sc_ssl;

	SPLAY_HEAD(batchtree, batch)		batch_queue;
	SPLAY_HEAD(mdaproctree, mdaproc)	mdaproc_queue;
};

/* aliases.c */
int		is_alias(struct path *);

/* atomic.c */
ssize_t		atomic_read(int, void *, size_t);
ssize_t		atomic_write(int, const void *, size_t);
ssize_t		atomic_printfd(int, const char *, ...);

/* log.c */
void		log_init(int);
void		log_warn(const char *, ...);
void		log_warnx(const char *, ...);
void		log_info(const char *, ...);
void		log_debug(const char *, ...);
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

/* lka.c */
pid_t		 lka(struct smtpd *);

/* mfa.c */
pid_t		 mfa(struct smtpd *);
int		 msg_cmp(struct message *, struct message *);
SPLAY_PROTOTYPE(msgtree, message, nodes, msg_cmp);

/* queue.c */
pid_t		 queue(struct smtpd *);
u_int64_t	 queue_generate_id(void);
int		 batch_cmp(struct batch *, struct batch *);
struct batch    *batch_by_id(struct smtpd *, u_int64_t);
struct message	*message_by_id(struct smtpd *, struct batch *, u_int64_t);
int		 queue_remove_batch_message(struct smtpd *, struct batch *, struct message *);
SPLAY_PROTOTYPE(batchtree, batch, b_nodes, batch_cmp);

/* mda.c */
pid_t		 mda(struct smtpd *);
int		 mdaproc_cmp(struct mdaproc *, struct mdaproc *);
SPLAY_PROTOTYPE(mdaproctree, mdaproc, mdaproc_nodes, mdaproc_cmp);

/* mta.c */
pid_t		 mta(struct smtpd *);

/* control.c */
pid_t		 control(struct smtpd *);
void		 session_socket_blockmode(int, enum blockmodes);

/* smtp.c */
pid_t		 smtp(struct smtpd *);
void		 smtp_listener_setup(struct smtpd *, struct listener *);

/* smtp_session.c */
void		 session_init(struct listener *, struct session *);
void		 session_read(struct bufferevent *, void *);
void		 session_write(struct bufferevent *, void *);
void		 session_error(struct bufferevent *, short, void *);
int		 session_cmp(struct session *, struct session *);
void		 session_msg_submit(struct session *);
void		 session_pickup(struct session *, struct submit_status *);
void		 session_destroy(struct session *);
SPLAY_PROTOTYPE(sessiontree, session, s_nodes, session_cmp);

/* store.c */
int store_write_header(struct batch *, struct message *, FILE *);
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
void		 init_peers(struct smtpd *);
void		 config_peers(struct smtpd *, struct peer *, u_int);

/* parse.y */
int		 parse_config(struct smtpd *, const char *, int);
int		 cmdline_symset(char *);

/* ssl.c */
void	 ssl_init(void);
void	 ssl_transaction(struct session *);

void	 ssl_session_init(struct session *);
void	 ssl_session_destroy(struct session *);
int	 ssl_load_certfile(struct smtpd *, const char *);
void	 ssl_setup(struct smtpd *, struct listener *);
int	 ssl_cmp(struct ssl *, struct ssl *);
SPLAY_PROTOTYPE(ssltree, ssl, ssl_nodes, ssl_cmp);

/* ssl_privsep.c */
int	 ssl_ctx_use_private_key(void *, char *, off_t);
int	 ssl_ctx_use_certificate_chain(void *, char *, off_t);

/* smtpd.c */
struct map	*map_find(struct smtpd *, objid_t);
struct map	*map_findbyname(struct smtpd *, const char *);
