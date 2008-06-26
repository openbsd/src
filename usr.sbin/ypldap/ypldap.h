/*
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

#define YPLDAP_USER		"_ospfd"
#define YPLDAP_CONF_FILE	"/etc/ypldap.conf"
#define DEFAULT_INTERVAL	600
#define _PATH_LDAPBIND_SOCK	"/var/run/ypldap.sock"
#define LINE_WIDTH		1024
#define FILTER_WIDTH		128
#define ATTR_WIDTH		32

/* buffer */
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

#define IMSG_HEADER_SIZE	sizeof(struct imsg_hdr)
#define MAX_IMSGSIZE		8192

struct buf_read {
	u_char			 buf[MAX_IMSGSIZE];
	u_char			*rptr;
	size_t			 wpos;
};

struct imsg_fd {
	TAILQ_ENTRY(imsg_fd)	entry;
	int			fd;
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
};

enum imsg_type {
	IMSG_NONE = 0,
	IMSG_GETPWENT = 1,	/* sends nothing expects nothing */
	IMSG_SETPWENT = 2,	/* sends nothing expects nothing */
	IMSG_GETPWNAM = 3,	/* sends a name expects a line */ 
	IMSG_GETPWID = 4,	/* sends a uid_t expects a line */
	IMSG_GETGRENT = 5,	/* sends nothing expects nothing */
	IMSG_SETGRENT = 6,	/* sends nothing expects nothing */
	IMSG_GETGRNAM = 7,	/* sends a name expects a line */ 
	IMSG_GETGRID = 8,	/* sends a uid_t expects a line */
	IMSG_CONF_START,
	IMSG_CONF_IDM,
	IMSG_CONF_SERVER,
	IMSG_CONF_END,
	IMSG_START_UPDATE,
	IMSG_END_UPDATE,
	IMSG_TRASH_UPDATE,
	IMSG_PW_ENTRY,
	IMSG_GRP_ENTRY
};

struct imsg_hdr {
	u_int16_t	 type;
	u_int16_t	 len;
	u_int32_t	 peerid;
	pid_t		 pid;
};

struct imsg {
	struct imsg_hdr	 hdr;
	void		*data;
};

enum blockmodes {
	BM_NORMAL,
	BM_NONBLOCK,
};

enum {
	PROC_MAIN,
	PROC_CLIENT
} lb_process;

union req {
	uid_t	uid;
	gid_t	gid;
	char	nam[_PW_NAME_LEN+1];
};

struct userent {
	RB_ENTRY(userent)		 ue_name_node;
	RB_ENTRY(userent)		 ue_uid_node;
	uid_t				 ue_uid;
	char				*ue_line;
};

struct groupent {
	RB_ENTRY(groupent)		 ge_name_node;
	RB_ENTRY(groupent)		 ge_gid_node;
	gid_t				 ge_gid;
	char				*ge_line;
};

/*
 * beck, djm, dlg: pay attention to the struct name
 */
struct idm {
	TAILQ_ENTRY(idm)		 idm_entry;
	char				 idm_name[MAXHOSTNAMELEN];
#define F_SSL				 0x00100000
#define F_CONFIGURING			 0x00200000
#define F_NEEDAUTH			 0x00400000
#define F_FIXED_ATTR(n)			 (1<<n)
	u_int32_t			 idm_flags; /* lower 20 reserved */
	in_port_t			 idm_port;
	char				 idm_binddn[LINE_WIDTH];
	char				 idm_bindcred[LINE_WIDTH];
#define FILTER_USER			 1
#define FILTER_GROUP			 0
	char				 idm_filters[2][FILTER_WIDTH];
#define ATTR_NAME			 0
#define ATTR_PASSWD			 1
#define ATTR_UID			 2
#define ATTR_GID			 3
#define ATTR_CLASS			 4
#define ATTR_CHANGE			 5
#define ATTR_EXPIRE			 6
#define ATTR_GECOS			 7
#define ATTR_DIR			 8
#define ATTR_SHELL			 9
#define ATTR_GR_NAME			 10
#define ATTR_GR_PASSWD			 11
#define ATTR_GR_GID			 12
#define ATTR_GR_MEMBERS			 13
#define ATTR_MAX			 10
#define ATTR_GR_MIN			 10
#define ATTR_GR_MAX			 14
	char				 idm_attrs[14][ATTR_WIDTH];
	struct env			*idm_env;
	struct event			 idm_ev;
#ifdef SSL
	struct ssl			*idm_ssl;
#endif
	
};

struct idm_req {
	union {
		uid_t			 ik_uid;
		uid_t			 ik_gid;
	}				 ir_key;
	char				 ir_line[LINE_WIDTH];
};

struct env {
#define YPLDAP_OPT_VERBOSE		 0x01
#define YPLDAP_OPT_NOACTION		 0x02
	u_int8_t			 sc_opts;
#define YPMAP_PASSWD_BYNAME		 0x00000001
#define YPMAP_PASSWD_BYUID		 0x00000002
#define YPMAP_MASTER_PASSWD_BYNAME	 0x00000004
#define YPMAP_MASTER_PASSWD_BYUID	 0x00000008
#define YPMAP_GROUP_BYNAME		 0x00000010
#define YPMAP_GROUP_BYGID		 0x00000020
	u_int32_t			 sc_flags;

	char				 sc_domainname[MAXHOSTNAMELEN];
	struct timeval			 sc_conf_tv;
	struct event			 sc_conf_ev;
	TAILQ_HEAD(idm_list, idm)	 sc_idms;
	struct imsgbuf			*sc_ibuf;

	RB_HEAD(user_name_tree,userent)	 *sc_user_names;
	RB_HEAD(user_uid_tree,userent)	 sc_user_uids;
	RB_HEAD(group_name_tree,groupent)*sc_group_names;
	RB_HEAD(group_gid_tree,groupent) sc_group_gids;
	struct user_name_tree		 *sc_user_names_t;
	struct group_name_tree		 *sc_group_names_t;
	size_t				 sc_user_line_len;
	size_t				 sc_group_line_len;
	char				*sc_user_lines;
	char				*sc_group_lines;

	struct yp_data			*sc_yp;
};

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
	    void *, u_int16_t);
#if 0
int	 imsg_get_fd(struct imsgbuf *);
int	 imsg_composev(struct imsgbuf *, enum imsg_type, u_int32_t,
	    pid_t, const struct iovec *, int);
int	 imsg_flush(struct imsgbuf *);
#endif
struct buf *imsg_create(struct imsgbuf *, enum imsg_type, u_int32_t, pid_t,
	    u_int16_t);
int	 imsg_add(struct buf *, void *, u_int16_t);
int	 imsg_close(struct imsgbuf *, struct buf *);
void	 imsg_free(struct imsg *);
void	 imsg_event_add(struct imsgbuf *); /* needs to be provided externally */
void	 imsg_clear(struct imsgbuf *);

/* log.c */
void		 log_init(int);
void		 log_warn(const char *, ...);
void		 log_warnx(const char *, ...);
void		 log_info(const char *, ...);
void		 log_debug(const char *, ...);
__dead void	 fatal(const char *);
__dead void	 fatalx(const char *);

/* parse.y */
int		 parse_config(struct env *, const char *, int);
int		 cmdline_symset(char *);

/* listener.c */
void		 listener_setup(struct env *);
void		 listener_init(struct env *);

/* ldapclient.c */
pid_t		 ldapclient(int []);

/* ypldap.c */
void		 purge_config(struct env *);

/* entries.c */
void		 flatten_entries(struct env *);
int		 userent_name_cmp(struct userent *, struct userent *);
int		 userent_uid_cmp(struct userent *, struct userent *);
int		 groupent_name_cmp(struct groupent *, struct groupent *);
int		 groupent_gid_cmp(struct groupent *, struct groupent *);
RB_PROTOTYPE(	 user_name_tree, userent, ue_name_node, userent_name_cmp);
RB_PROTOTYPE(	 user_uid_tree, userent, ue_uid_node, userent_uid_cmp);
RB_PROTOTYPE(	 group_name_tree, groupent, ge_name_node, groupent_name_cmp);
RB_PROTOTYPE(	 group_gid_tree, groupent, ge_gid_node, groupent_gid_cmp);

/* yp.c */
void		 yp_init(struct env *);
void		 yp_enable_events(void);
