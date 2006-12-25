/*
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@spootnik.org>
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

#define CONF_FILE	"/etc/hostated.conf"
#define HOSTATED_SOCKET	"/var/run/hostated.sock"
#define PF_SOCKET	"/dev/pf"
#define HOSTATED_USER	"_hostated"
#define HOSTATED_ANCHOR	"hostated"
#define CHECK_TIMEOUT	200
#define CHECK_INTERVAL	10
#define EMPTY_TABLE	UINT_MAX
#define EMPTY_ID	UINT_MAX
#define TABLE_NAME_SIZE	32
#define	TAG_NAME_SIZE	64
#define SRV_NAME_SIZE	64
#define MAX_NAME_SIZE	64
#define SRV_MAX_VIRTS	16

#define SMALL_READ_BUF_SIZE	1024
#define READ_BUF_SIZE		65535

/* buffer */
struct buf {
	TAILQ_ENTRY(buf)	 entry;
	u_char			*buf;
	size_t			 size;
	size_t			 max;
	size_t			 wpos;
	size_t			 rpos;
};

struct msgbuf {
	TAILQ_HEAD(, buf)	 bufs;
	u_int32_t		 queued;
	int			 fd;
};

#define IMSG_HEADER_SIZE	sizeof(struct imsg_hdr)
#define MAX_IMSGSIZE		8192

struct buf_read {
	u_char			 buf[READ_BUF_SIZE];
	u_char			*rptr;
	size_t			 wpos;
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
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_OK,		/* answer to hostatectl requests */
	IMSG_CTL_FAIL,
	IMSG_CTL_END,
	IMSG_CTL_SERVICE,
	IMSG_CTL_TABLE,
	IMSG_CTL_HOST,
	IMSG_CTL_SHOW_SUM,	/* hostatectl requests */
	IMSG_CTL_SERVICE_ENABLE,
	IMSG_CTL_SERVICE_DISABLE,
	IMSG_CTL_TABLE_ENABLE,
	IMSG_CTL_TABLE_DISABLE,
	IMSG_CTL_HOST_ENABLE,
	IMSG_CTL_HOST_DISABLE,
	IMSG_CTL_SHUTDOWN,
	IMSG_CTL_RELOAD,
	IMSG_SERVICE_ENABLE,	/* notifies from pfe to hce */
	IMSG_SERVICE_DISABLE,
	IMSG_TABLE_ENABLE,
	IMSG_TABLE_DISABLE,
	IMSG_HOST_ENABLE,
	IMSG_HOST_DISABLE,
	IMSG_TABLE_STATUS,	/* notifies from hce to pfe */
	IMSG_HOST_STATUS,
	IMSG_SYNC
};

struct imsg_hdr {
	enum imsg_type	 type;
	u_int16_t	 len;
	u_int32_t	 peerid;
	pid_t		 pid;
};

struct imsg {
	struct imsg_hdr	 hdr;
	void		*data;
};

typedef u_int32_t objid_t;

struct ctl_status {
	objid_t		 id;
	int		 up;
};

struct ctl_id {
	objid_t		 id;
	char		 name[MAX_NAME_SIZE];
};

struct ctl_icmp_event {
	struct hostated	*env;
	int		 icmp_sock;
	int		 icmp6_sock;
	int		 has_icmp4;
	int		 has_icmp6;
	int		 last_up;
	struct event	 ev;
	struct timeval	 tv_start;
};

struct ctl_tcp_event {
	int		 s;
	struct buf	*buf;
	struct host 	*host;
	struct table	*table;
	struct timeval	 tv_start;
};

struct address {
	struct sockaddr_storage	 ss;
	in_port_t		 port;
	char			 ifname[IFNAMSIZ];
	TAILQ_ENTRY(address)	 entry;
};
TAILQ_HEAD(addresslist, address);

#define F_DISABLE		0x01
#define F_BACKUP		0x02
#define F_CHECK_DONE		0x02 /* reused for host */
#define F_USED			0x04
#define F_ACTIVE_RULESET	0x04 /* reused for service */
#define F_DOWN			0x08
#define F_ADD			0x10
#define F_DEL			0x20
#define F_CHANGED		0x40

struct host {
	u_int8_t		 flags;
	objid_t			 id;
	objid_t			 tableid;
	char			*tablename;
	char			 name[MAXHOSTNAMELEN];
	int			 up;
	int			 last_up;
	struct sockaddr_storage	 ss;
	struct ctl_tcp_event	 cte;
	TAILQ_ENTRY(host)	 entry;
};
TAILQ_HEAD(hostlist, host);

#define HOST_DOWN		-1
#define HOST_UNKNOWN		0
#define HOST_UP			1

struct table {
	objid_t			 id;
	objid_t			 serviceid;
	u_int8_t		 flags;
	int			 check;
	int			 up;
	in_port_t		 port;
	int			 retcode;
	struct timeval		 timeout;
	char			 name[TABLE_NAME_SIZE];
	char			 path[MAXPATHLEN];
	char			 digest[41]; /* length of sha1 digest * 2 */
	struct hostlist		 hosts;
	TAILQ_ENTRY(table)	 entry;
};
TAILQ_HEAD(tablelist, table);

#define CHECK_NOCHECK		0
#define CHECK_ICMP		1
#define CHECK_TCP		2
#define CHECK_HTTP_CODE		3
#define CHECK_HTTP_DIGEST	4

struct service {
	objid_t			 id;
	u_int8_t		 flags;
	in_port_t		 port;
	char			 name[SRV_NAME_SIZE];
	char			 tag[TAG_NAME_SIZE];
	struct addresslist	 virts;
	struct table		*table;
	struct table		*backup; /* use this if no host up */
	TAILQ_ENTRY(service)	 entry;
};
TAILQ_HEAD(servicelist, service);

enum {
	PROC_MAIN,
	PROC_PFE,
	PROC_HCE
} hostated_process;

struct hostated {
	u_int8_t		 opts;
	struct pfdata		*pf;
	int			 interval;
	int			 icmp_sock;
	int			 icmp6_sock;
	int			 tablecount;
	int			 servicecount;
	struct timeval		 timeout;
	struct table		 empty_table;
	struct event		 ev;
	struct tablelist	 tables;
	struct servicelist	 services;
	struct ctl_icmp_event	 cie;
};

#define HOSTATED_OPT_VERBOSE	 0x01
#define HOSTATED_OPT_NOACTION	 0x04

/* initially control.h */
struct {
	struct event	 ev;
	int		 fd;
} control_state;

enum blockmodes {
	BM_NORMAL,
	BM_NONBLOCK
};

struct ctl_conn {
	TAILQ_ENTRY(ctl_conn)	 entry;
	struct imsgbuf		 ibuf;

};
TAILQ_HEAD(ctl_connlist, ctl_conn);

/* control.c */
int     control_init(void);
int     control_listen(void);
void    control_accept(int, short, void *);
void    control_dispatch_imsg(int, short, void *);
int     control_imsg_relay(struct imsg *);
void    control_cleanup(void);

void    session_socket_blockmode(int, enum blockmodes);

extern  struct ctl_connlist ctl_conns;

/* parse.y */
int	parse_config(struct hostated *, const char *, int);

/* log.c */
void	log_init(int);
void	log_warn(const char *, ...);
void	log_warnx(const char *, ...);
void	log_info(const char *, ...);
void	log_debug(const char *, ...);
void	fatal(const char *);
void	fatalx(const char *);

/* buffer.c */
struct buf	*buf_open(size_t);
struct buf	*buf_dynamic(size_t, size_t);
int		 buf_add(struct buf *, void *, size_t);
void		*buf_reserve(struct buf *, size_t);
void		*buf_seek(struct buf *, size_t, size_t);
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
struct buf *imsg_create(struct imsgbuf *, enum imsg_type, u_int32_t, pid_t,
	    u_int16_t);
int	 imsg_add(struct buf *, void *, u_int16_t);
int	 imsg_close(struct imsgbuf *, struct buf *);
void	 imsg_free(struct imsg *);
void	 imsg_event_add(struct imsgbuf *); /* needs to be provided externally */

/* pfe.c */
pid_t	 pfe(struct hostated *, int [2], int [2], int [2]);
void	 show(struct ctl_conn *);
int	 enable_service(struct ctl_conn *, struct ctl_id *);
int	 enable_table(struct ctl_conn *, struct ctl_id *);
int	 enable_host(struct ctl_conn *, struct ctl_id *);
int	 disable_service(struct ctl_conn *, struct ctl_id *);
int	 disable_table(struct ctl_conn *, struct ctl_id *);
int	 disable_host(struct ctl_conn *, struct ctl_id *);

/* pfe_filter.c */
void	 init_filter(struct hostated *);
void	 init_tables(struct hostated *);
void	 flush_table(struct hostated *, struct service *);
void	 sync_table(struct hostated *, struct service *, struct table *);
void	 sync_ruleset(struct hostated *, struct service *, int);
void	 flush_rulesets(struct hostated *);

/* hce.c */
pid_t	 hce(struct hostated *, int [2], int [2], int [2]);
void	 hce_notify_done(struct host *, const char *);

/* check_icmp.c */
void	 schedule_icmp(struct ctl_icmp_event *, struct table *);
void	 check_icmp(struct ctl_icmp_event *);

/* check_tcp.c */
void	 check_tcp(struct ctl_tcp_event *);

/* check_http.c */
void	 send_http_request(struct ctl_tcp_event *);

/* hostated.c */
struct host	*host_find(struct hostated *, objid_t);
struct table	*table_find(struct hostated *, objid_t);
struct service	*service_find(struct hostated *, objid_t);
struct host	*host_findbyname(struct hostated *, const char *);
struct table	*table_findbyname(struct hostated *, const char *);
struct service	*service_findbyname(struct hostated *, const char *);
