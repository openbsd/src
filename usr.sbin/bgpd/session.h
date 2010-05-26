/*	$OpenBSD: session.h,v 1.107 2010/05/26 13:56:07 nicm Exp $ */

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
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

#define	MAX_BACKLOG			5
#define	INTERVAL_CONNECTRETRY		120
#define	INTERVAL_HOLD_INITIAL		240
#define	INTERVAL_HOLD			90
#define	INTERVAL_IDLE_HOLD_INITIAL	30
#define	INTERVAL_HOLD_CLONED		3600
#define	INTERVAL_HOLD_DEMOTED		60
#define	MAX_IDLE_HOLD			3600
#define	MSGSIZE_HEADER			19
#define	MSGSIZE_HEADER_MARKER		16
#define	MSGSIZE_NOTIFICATION_MIN	21	/* 19 hdr + 1 code + 1 sub */
#define	MSGSIZE_OPEN_MIN		29
#define	MSGSIZE_UPDATE_MIN		23
#define	MSGSIZE_KEEPALIVE		MSGSIZE_HEADER
#define	MSGSIZE_RREFRESH		MSGSIZE_HEADER + 4
#define	MSG_PROCESS_LIMIT		25
#define	SESSION_CLEAR_DELAY		5

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

enum blockmodes {
	BM_NORMAL,
	BM_NONBLOCK
};

enum msg_type {
	OPEN = 1,
	UPDATE,
	NOTIFICATION,
	KEEPALIVE,
	RREFRESH
};

enum suberr_header {
	ERR_HDR_SYNC = 1,
	ERR_HDR_LEN,
	ERR_HDR_TYPE
};

enum suberr_open {
	ERR_OPEN_VERSION = 1,
	ERR_OPEN_AS,
	ERR_OPEN_BGPID,
	ERR_OPEN_OPT,
	ERR_OPEN_AUTH,
	ERR_OPEN_HOLDTIME,
	ERR_OPEN_CAPA
};

enum opt_params {
	OPT_PARAM_NONE,
	OPT_PARAM_AUTH,
	OPT_PARAM_CAPABILITIES
};

enum capa_codes {
	CAPA_NONE,
	CAPA_MP,
	CAPA_REFRESH,
	CAPA_RESTART = 64,
	CAPA_AS4BYTE = 65
};

struct bgp_msg {
	struct ibuf	*buf;
	enum msg_type	 type;
	u_int16_t	 len;
};

struct msg_header {
	u_char			 marker[MSGSIZE_HEADER_MARKER];
	u_int16_t		 len;
	u_int8_t		 type;
};

struct msg_open {
	struct msg_header	 header;
	u_int32_t		 bgpid;
	u_int16_t		 myas;
	u_int16_t		 holdtime;
	u_int8_t		 version;
	u_int8_t		 optparamlen;
};

struct bgpd_sysdep {
	u_int8_t		no_pfkey;
	u_int8_t		no_md5sig;
};

struct ctl_conn {
	TAILQ_ENTRY(ctl_conn)	entry;
	struct imsgbuf		ibuf;
	int			restricted;
};

TAILQ_HEAD(ctl_conns, ctl_conn)	ctl_conns;

struct peer_stats {
	u_int64_t		 msg_rcvd_open;
	u_int64_t		 msg_rcvd_update;
	u_int64_t		 msg_rcvd_notification;
	u_int64_t		 msg_rcvd_keepalive;
	u_int64_t		 msg_rcvd_rrefresh;
	u_int64_t		 msg_sent_open;
	u_int64_t		 msg_sent_update;
	u_int64_t		 msg_sent_notification;
	u_int64_t		 msg_sent_keepalive;
	u_int64_t		 msg_sent_rrefresh;
	u_int64_t		 prefix_rcvd_update;
	u_int64_t		 prefix_rcvd_withdraw;
	u_int64_t		 prefix_sent_update;
	u_int64_t		 prefix_sent_withdraw;
	time_t			 last_updown;
	time_t			 last_read;
	u_int32_t		 prefix_cnt;
	u_int8_t		 last_sent_errcode;
	u_int8_t		 last_sent_suberr;
};

enum Timer {
	Timer_None,
	Timer_ConnectRetry,
	Timer_Keepalive,
	Timer_Hold,
	Timer_IdleHold,
	Timer_IdleHoldReset,
	Timer_CarpUndemote,
	Timer_Max
};

struct peer_timer {
	TAILQ_ENTRY(peer_timer)	entry;
	enum Timer		type;
	time_t			val;
};

TAILQ_HEAD(peer_timer_head, peer_timer);

struct peer {
	struct peer_config	 conf;
	struct peer_stats	 stats;
	struct {
		struct capabilities	ann;
		struct capabilities	peer;
		struct capabilities	neg;
	}			 capa;
	struct {
		struct bgpd_addr	local_addr;
		u_int32_t		spi_in;
		u_int32_t		spi_out;
		enum auth_method	method;
		u_int8_t		established;
	} auth;
	struct sockaddr_storage	 sa_local;
	struct sockaddr_storage	 sa_remote;
	struct peer_timer_head	 timers;
	struct msgbuf		 wbuf;
	struct ibuf_read	*rbuf;
	struct peer		*next;
	int			 fd;
	int			 lasterr;
	u_int			 errcnt;
	u_int			 IdleHoldTime;
	u_int32_t		 remote_bgpid;
	enum session_state	 state;
	enum session_state	 prev_state;
	u_int16_t		 short_as;
	u_int16_t		 holdtime;
	u_int8_t		 depend_ok;
	u_int8_t		 demoted;
	u_int8_t		 passive;
};

extern struct peer	*peers;

struct ctl_timer {
	enum Timer	type;
	time_t		val;
};

/* session.c */
void		 session_socket_blockmode(int, enum blockmodes);
pid_t		 session_main(int[2], int[2], int[2], int[2], char *, char *);
void		 bgp_fsm(struct peer *, enum session_events);
int		 session_neighbor_rrefresh(struct peer *p);
struct peer	*getpeerbyaddr(struct bgpd_addr *);
struct peer	*getpeerbydesc(const char *);
int		 imsg_compose_parent(int, u_int32_t, pid_t, void *, u_int16_t);
int		 imsg_compose_rde(int, pid_t, void *, u_int16_t);
void	 	 session_stop(struct peer *, u_int8_t);

/* log.c */
char		*log_fmt_peer(const struct peer_config *);
void		 log_statechange(struct peer *, enum session_state,
		    enum session_events);
void		 log_notification(const struct peer *, u_int8_t, u_int8_t,
		    u_char *, u_int16_t);
void		 log_conn_attempt(const struct peer *, struct sockaddr *);

/* parse.y */
int	 parse_config(char *, struct bgpd_config *, struct mrt_head *,
	    struct peer **, struct network_head *, struct filter_head *,
	    struct rdomain_head *);

/* config.c */
int	 merge_config(struct bgpd_config *, struct bgpd_config *,
	    struct peer *, struct listen_addrs *);
void	 prepare_listeners(struct bgpd_config *);
int	 get_mpe_label(struct rdomain *);

/* rde.c */
pid_t	 rde_main(int[2], int[2], int[2], int[2], int);

/* control.c */
int	control_init(int, char *);
int	control_listen(int);
void	control_shutdown(int);
int	control_dispatch_msg(struct pollfd *, u_int *);
unsigned int	control_accept(int, int);

/* pfkey.c */
int	pfkey_establish(struct peer *);
int	pfkey_remove(struct peer *);
int	pfkey_init(struct bgpd_sysdep *);

/* printconf.c */
void	print_config(struct bgpd_config *, struct rib_names *,
	    struct network_head *, struct peer *, struct filter_head *,
	    struct mrt_head *, struct rdomain_head *);

/* carp.c */
int	 carp_demote_init(char *, int);
void	 carp_demote_shutdown(void);
int	 carp_demote_get(char *);
int	 carp_demote_set(char *, int);

/* timer.c */
struct peer_timer	*timer_get(struct peer *, enum Timer);
struct peer_timer	*timer_nextisdue(struct peer *);
time_t			 timer_nextduein(struct peer *);
int			 timer_running(struct peer *, enum Timer, time_t *);
void			 timer_set(struct peer *, enum Timer, u_int);
void			 timer_stop(struct peer *, enum Timer);
void			 timer_remove(struct peer *, enum Timer);
void			 timer_remove_all(struct peer *);
