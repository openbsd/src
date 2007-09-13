/*	$OpenBSD: ntpd.h,v 1.87 2007/09/13 14:34:36 pyr Exp $ */

/*
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
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdarg.h>

#include "ntp.h"

#define	NTPD_USER	"_ntp"
#define	CONFFILE	"/etc/ntpd.conf"
#define DRIFTFILE	"/var/db/ntpd.drift"

#define	READ_BUF_SIZE		8192

#define	NTPD_OPT_VERBOSE	0x0001
#define	NTPD_OPT_VERBOSE2	0x0002

#define	INTERVAL_QUERY_NORMAL		30	/* sync to peers every n secs */
#define	INTERVAL_QUERY_PATHETIC		60
#define	INTERVAL_QUERY_AGGRESSIVE	5

#define	TRUSTLEVEL_BADPEER		6
#define	TRUSTLEVEL_PATHETIC		2
#define	TRUSTLEVEL_AGGRESSIVE		8
#define	TRUSTLEVEL_MAX			10

#define	MAX_SERVERS_DNS			8

#define	QSCALE_OFF_MIN			0.001
#define	QSCALE_OFF_MAX			0.050

#define	QUERYTIME_MAX		15	/* single query might take n secs max */
#define	OFFSET_ARRAY_SIZE	8
#define	SENSOR_OFFSETS		7
#define	SETTIME_MIN_OFFSET	180	/* min offset for settime at start */
#define	SETTIME_TIMEOUT		15	/* max seconds to wait with -s */
#define	LOG_NEGLIGEE		32	/* negligible drift to not log (ms) */
#define	FREQUENCY_SAMPLES	8	/* samples for est. of permanent drift */
#define	MAX_FREQUENCY_ADJUST	128e-5	/* max correction per iteration */


#define	SENSOR_DATA_MAXAGE	(15*60)
#define	SENSOR_QUERY_INTERVAL	30
#define	SENSOR_SCAN_INTERVAL	(5*60)

enum client_state {
	STATE_NONE,
	STATE_DNS_INPROGRESS,
	STATE_DNS_TEMPFAIL,
	STATE_DNS_DONE,
	STATE_QUERY_SENT,
	STATE_REPLY_RECEIVED
};

struct listen_addr {
	TAILQ_ENTRY(listen_addr)	 entry;
	struct sockaddr_storage		 sa;
	int				 fd;
};

struct ntp_addr {
	struct ntp_addr		*next;
	struct sockaddr_storage	 ss;
};

struct ntp_addr_wrap {
	char			*name;
	struct ntp_addr		*a;
	u_int8_t		 pool;
};

struct ntp_status {
	double		rootdelay;
	double		rootdispersion;
	double		reftime;
	u_int32_t	refid;
	u_int32_t	refid4;
	u_int32_t	send_refid;
	u_int8_t	synced;
	u_int8_t	leap;
	int8_t		precision;
	u_int8_t	poll;
	u_int8_t	stratum;
};

struct ntp_offset {
	struct ntp_status	status;
	double			offset;
	double			delay;
	double			error;
	time_t			rcvd;
	u_int8_t		good;
};

struct ntp_peer {
	TAILQ_ENTRY(ntp_peer)		 entry;
	struct ntp_addr_wrap		 addr_head;
	struct ntp_addr			*addr;
	struct ntp_query		*query;
	struct ntp_offset		 reply[OFFSET_ARRAY_SIZE];
	struct ntp_offset		 update;
	enum client_state		 state;
	time_t				 next;
	time_t				 deadline;
	u_int32_t			 id;
	u_int8_t			 shift;
	u_int8_t			 trustlevel;
	u_int8_t			 weight;
	int				 lasterror;
};

struct ntp_sensor {
	TAILQ_ENTRY(ntp_sensor)		 entry;
	struct ntp_offset		 offsets[SENSOR_OFFSETS];
	struct ntp_offset		 update;
	time_t				 next;
	time_t				 last;
	char				*device;
	int				 sensordevid;
	int				 correction;
	u_int8_t			 weight;
	u_int8_t			 shift;
};

struct ntp_conf_sensor {
	TAILQ_ENTRY(ntp_conf_sensor)		 entry;
	char					*device;
	int					 correction;
	u_int8_t				 weight;
};

struct ntp_freq {
	double				overall_offset;
	double				x, y;
	double				xx, xy;
	int				samples;
	u_int				num;
};

struct ntpd_conf {
	TAILQ_HEAD(listen_addrs, listen_addr)		listen_addrs;
	TAILQ_HEAD(ntp_peers, ntp_peer)			ntp_peers;
	TAILQ_HEAD(ntp_sensors, ntp_sensor)		ntp_sensors;
	TAILQ_HEAD(ntp_conf_sensors, ntp_conf_sensor)	ntp_conf_sensors;
	struct ntp_status				status;
	struct ntp_freq					freq;
	u_int8_t					listen_all;
	u_int8_t					settime;
	u_int8_t					debug;
	u_int32_t					scale;
	u_int8_t					noaction;
};

struct buf {
	TAILQ_ENTRY(buf)	 entry;
	u_char			*buf;
	size_t			 size;
	size_t			 wpos;
	size_t			 rpos;
};

struct msgbuf {
	TAILQ_HEAD(, buf)	 bufs;
	u_int32_t		 queued;
	int			 fd;
};

struct buf_read {
	size_t			 wpos;
	u_char			 buf[READ_BUF_SIZE];
	u_char			*rptr;
};

/* ipc messages */

#define	IMSG_HEADER_SIZE	sizeof(struct imsg_hdr)
#define	MAX_IMSGSIZE		8192

struct imsgbuf {
	int			fd;
	pid_t			pid;
	struct buf_read		r;
	struct msgbuf		w;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_ADJTIME,
	IMSG_ADJFREQ,
	IMSG_SETTIME,
	IMSG_HOST_DNS
};

struct imsg_hdr {
	enum imsg_type	type;
	u_int32_t	peerid;
	pid_t		pid;
	u_int16_t	len;
};

struct imsg {
	struct imsg_hdr	 hdr;
	void		*data;
};

/* prototypes */
/* log.c */
void		 log_init(int);
void		 vlog(int, const char *, va_list);
void		 log_warn(const char *, ...);
void		 log_warnx(const char *, ...);
void		 log_info(const char *, ...);
void		 log_debug(const char *, ...);
void		 fatal(const char *);
void		 fatalx(const char *);
const char *	 log_sockaddr(struct sockaddr *);

/* buffer.c */
struct buf	*buf_open(size_t);
int		 buf_add(struct buf *, void *, size_t);
int		 buf_close(struct msgbuf *, struct buf *);
void		 buf_free(struct buf *);
void		 msgbuf_init(struct msgbuf *);
void		 msgbuf_clear(struct msgbuf *);
int		 msgbuf_write(struct msgbuf *);

/* imsg.c */
void	 imsg_init(struct imsgbuf *, int);
int	 imsg_read(struct imsgbuf *);
int	 imsg_get(struct imsgbuf *, struct imsg *);
int	 imsg_compose(struct imsgbuf *, enum imsg_type, u_int32_t, pid_t,
	    void *, u_int16_t);
struct buf	*imsg_create(struct imsgbuf *, enum imsg_type, u_int32_t, pid_t,
		    u_int16_t);
int	 imsg_add(struct buf *, void *, u_int16_t);
int	 imsg_close(struct imsgbuf *, struct buf *);
void	 imsg_free(struct imsg *);

/* ntp.c */
pid_t	 ntp_main(int[2], struct ntpd_conf *);
int	 priv_adjtime(void);
void	 priv_settime(double);
void	 priv_host_dns(char *, u_int32_t);
int	 offset_compare(const void *, const void *);
extern struct ntpd_conf *conf;

/* parse.y */
int	 parse_config(const char *, struct ntpd_conf *);

/* config.c */
int			 host(const char *, struct ntp_addr **);
int			 host_dns(const char *, struct ntp_addr **);
struct ntp_peer		*new_peer(void);
struct ntp_conf_sensor	*new_sensor(char *);

/* ntp_msg.c */
int	ntp_getmsg(struct sockaddr *, char *, ssize_t, struct ntp_msg *);
int	ntp_sendmsg(int, struct sockaddr *, struct ntp_msg *, ssize_t, int);

/* server.c */
int	setup_listeners(struct servent *, struct ntpd_conf *, u_int *);
int	ntp_reply(int, struct sockaddr *, struct ntp_msg *, int);
int	server_dispatch(int, struct ntpd_conf *);

/* client.c */
int	client_peer_init(struct ntp_peer *);
int	client_addr_init(struct ntp_peer *);
int	client_nextaddr(struct ntp_peer *);
int	client_query(struct ntp_peer *);
int	client_dispatch(struct ntp_peer *, u_int8_t);
void	client_log_error(struct ntp_peer *, const char *, int);
void	update_scale(double);
time_t	scale_interval(time_t);
time_t	error_interval(void);
void	set_next(struct ntp_peer *, time_t);

/* util.c */
double			gettime_corrected(void);
double			getoffset(void);
double			gettime(void);
time_t			getmonotime(void);
void			d_to_tv(double, struct timeval *);
double			lfp_to_d(struct l_fixedpt);
struct l_fixedpt	d_to_lfp(double);
double			sfp_to_d(struct s_fixedpt);
struct s_fixedpt	d_to_sfp(double);

/* sensors.c */
void			sensor_init(void);
int			sensor_scan(void);
void			sensor_query(struct ntp_sensor *);
int			sensor_hotplugfd(void);
void			sensor_hotplugevent(int);
