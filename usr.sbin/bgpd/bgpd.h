/*	$OpenBSD: bgpd.h,v 1.12 2003/12/22 15:07:05 henning Exp $ */

/*
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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
#ifndef __BGPD_H__
#define __BGPD_H__

#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdarg.h>
#include <syslog.h>

#define	BGP_VERSION			4
#define	BGP_PORT			179
#define CONFFILE			"/etc/bgpd.conf"
#define	BGPD_USER			"_bgpd"
#define PEER_DESCR_LEN			32

#define	MAX_PKTSIZE			4096
#define	MIN_HOLDTIME			3

#define BGPD_OPT_VERBOSE		0x0001
#define BGPD_OPT_VERBOSE2		0x0002
#define BGPD_OPT_NOACTION		0x0004

enum {
	PROC_MAIN,
	PROC_SE,
	PROC_RDE
} bgpd_process;

enum session_state {
	STATE_NONE,
	STATE_IDLE,
	STATE_CONNECT,
	STATE_ACTIVE,
	STATE_OPENSENT,
	STATE_OPENCONFIRM,
	STATE_ESTABLISHED
};

enum session_events {
	EVNT_NONE,
	EVNT_START,
	EVNT_STOP,
	EVNT_CON_OPEN,
	EVNT_CON_CLOSED,
	EVNT_CON_OPENFAIL,
	EVNT_CON_FATAL,
	EVNT_TIMER_CONNRETRY,
	EVNT_TIMER_HOLDTIME,
	EVNT_TIMER_KEEPALIVE,
	EVNT_RCVD_OPEN,
	EVNT_RCVD_KEEPALIVE,
	EVNT_RCVD_UPDATE,
	EVNT_RCVD_NOTIFICATION
};

enum reconf_action {
	RECONF_NONE,
	RECONF_KEEP,
	RECONF_REINIT,
	RECONF_DELETE
};

struct buf {
	TAILQ_ENTRY(buf)	 entries;
	u_char			*buf;
	ssize_t			 size;
	ssize_t			 wpos;
	ssize_t			 rpos;
};

struct msgbuf {
	u_int32_t		 queued;
	int			 sock;
	TAILQ_HEAD(bufs, buf)	 bufs;
};

struct bgpd_config {
	int			 opts;
	u_int16_t		 as;
	u_int32_t		 bgpid;
	u_int16_t		 holdtime;
	u_int16_t		 min_holdtime;
	struct peer		*peers;
};

struct peer_buf_read {
	u_char			 buf[MAX_PKTSIZE];
	ssize_t			 read_len;
	u_int16_t		 pkt_len;
	u_int8_t		 type;
	u_char			*wptr;
	u_int8_t		 seen_hdr;
};

struct peer_config {
	u_int32_t		 id;
	char			 group[PEER_DESCR_LEN];
	char			 descr[PEER_DESCR_LEN];
	struct sockaddr_in	 remote_addr;
	struct sockaddr_in	 local_addr;
	u_int16_t		 remote_as;
	u_int8_t		 ebgp;		/* 1 = ebgp, 0 = ibgp */
	u_int8_t		 distance;	/* 1 = direct, >1 = multihop */
	enum reconf_action	 reconf_action;
};

struct peer {
	struct peer_config	 conf;
	u_int32_t		 remote_bgpid;
	u_int16_t		 holdtime;
	enum session_state	 state;
	time_t			 ConnectRetryTimer;
	time_t			 KeepaliveTimer;
	time_t			 HoldTimer;
	time_t			 StartTimer;
	u_int			 StartTimerInterval;
	int			 sock;
	int			 events;
	struct msgbuf		 wbuf;
	struct peer_buf_read	*rbuf;
	struct peer		*next;
};

#define MRT_FILE_LEN	512
enum mrtdump_type {
	MRT_NONE,
	MRT_TABLE_DUMP
/*
 *	MRT_UPDATE_START,
 *	MRT_SESSION_START,
 *	MRT_UPDATE_STOP,
 *	MRT_SESSION_STOP,
 */
};

enum mrtdump_state {
	MRT_STATE_OPEN,
	MRT_STATE_RUNNING,
	MRT_STATE_DONE,
	MRT_STATE_CLOSE,
	MRT_STATE_REOPEN
};

LIST_HEAD(mrt_config, mrtdump_config);

struct mrtdump_config {
	enum mrtdump_type	 type;
	u_int32_t		 id;
	struct msgbuf		 msgbuf;
	char			 name[MRT_FILE_LEN];	/* base file name */
	char			 file[MRT_FILE_LEN];	/* actual file name */
	time_t			 ReopenTimer;
	time_t			 ReopenTimerInterval;
	enum mrtdump_state	 state;
	LIST_ENTRY(mrtdump_config)
				 list;
};

/* ipc messages */

#define	IMSG_HEADER_SIZE	sizeof(struct imsg_hdr)
#define MAX_IMSGSIZE		8192

struct imsg_readbuf {
	u_char			 buf[MAX_IMSGSIZE];
	ssize_t			 read_len;
	u_int32_t		 peerid;
	u_int16_t		 pkt_len;
	u_int8_t		 type;
	u_char			*wptr;
	u_int8_t		 seen_hdr;
};

struct imsgbuf {
	int			sock;
	struct imsg_readbuf	r;
	struct msgbuf		w;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_RECONF_CONF,
	IMSG_RECONF_PEER,
	IMSG_RECONF_DONE,
	IMSG_UPDATE,
	IMSG_UPDATE_ERR,
	IMSG_SESSION_UP,
	IMSG_SESSION_DOWN,
	IMSG_MRT_REQ,
	IMSG_MRT_MSG,
	IMSG_MRT_END
};

struct imsg_hdr {
	enum imsg_type	type;
	u_int16_t	len;
	u_int32_t	peerid;
};

struct imsg {
	struct imsg_hdr	 hdr;
	void		*data;
};

/* error subcode for UPDATE; needed in SE and RDE */
enum suberr_update {
	ERR_UPD_ATTRLIST = 1,
	ERR_UPD_UNKNWN_WK_ATTR,
	ERR_UPD_MISSNG_WK_ATTR,
	ERR_UPD_ATTRFLAGS,
	ERR_UPD_ATTRLEN,
	ERR_UPD_ORIGIN,
	ERR_UPD_LOOP,
	ERR_UPD_NEXTHOP,
	ERR_UPD_OPTATTR,
	ERR_UPD_NETWORK,
	ERR_UPD_ASPATH
};

/* prototypes */
/* session.c */
int		 session_main(struct bgpd_config *, int[2], int[2]);

/* buffer.c */
struct buf	*buf_open(ssize_t);
int		 buf_add(struct buf *, void *, ssize_t);
void		*buf_reserve(struct buf *, ssize_t);
int		 buf_close(struct msgbuf *, struct buf *);
void		 buf_free(struct buf *);
void		 msgbuf_init(struct msgbuf *);
void		 msgbuf_clear(struct msgbuf *);
int		 msgbuf_write(struct msgbuf *);


/* log.c */
void		 log_init(int);
void		 logit(int, const char *, ...);
void		 vlog(int, const char *, va_list);
void		 log_err(struct peer *, const char *, ...);
void		 log_errx(struct peer *, const char *, ...);
void		 fatal(const char *, int);
void		 fatal_ensure(const char *, int, const char *);
void		 log_statechange(struct peer *, enum session_state,
		    enum session_events);
void		 log_notification(struct peer *, u_int8_t, u_int8_t,
		    u_char *, u_int16_t);
void		 log_conn_attempt(struct peer *, struct in_addr);

/* parse.y */
int	 cmdline_symset(char *);
int	 parse_config(char *, struct bgpd_config *, struct mrt_config *);

/* config.c */
int	 merge_config(struct bgpd_config *, struct bgpd_config *);

/* imsg.c */
void	 imsg_init(struct imsgbuf *, int);
int	 imsg_get(struct imsgbuf *, struct imsg *);
int	 imsg_compose(struct imsgbuf *, int, u_int32_t, void *, u_int16_t);
void	 imsg_free(struct imsg *);

/* rde.c */
int	 rde_main(struct bgpd_config *, int[2], int[2]);

/* mrt.c */
int	 mrt_mergeconfig(struct mrt_config *, struct mrt_config *);

#endif /* __BGPD_H__ */
