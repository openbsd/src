/*	$OpenBSD: ldpe.h,v 1.32 2014/10/25 03:23:49 lteo Exp $ */

/*
 * Copyright (c) 2004, 2005, 2008 Esben Norby <norby@openbsd.org>
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

#ifndef _LDPE_H_
#define _LDPE_H_

#define min(x,y) ((x) <= (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

TAILQ_HEAD(ctl_conns, ctl_conn)	ctl_conns;

struct hello_source {
	enum hello_type		 type;
	struct {
		struct iface	*iface;
		struct in_addr	 src_addr;
	}			 link;
	struct tnbr		*target;
};

struct adj {
	LIST_ENTRY(adj)		 nbr_entry;
	LIST_ENTRY(adj)		 iface_entry;
	struct nbr		*nbr;
	struct hello_source	 source;
	struct event		 inactivity_timer;
	u_int16_t		 holdtime;
	struct in_addr		 addr;
};

struct nbr;
struct tcp_conn {
	struct nbr		*nbr;
	int			 fd;
	struct ibuf_read	*rbuf;
	struct evbuf		 wbuf;
	struct event		 rev;
};

struct nbr {
	RB_ENTRY(nbr)		 id_tree, pid_tree;
	struct tcp_conn		*tcp;
	LIST_HEAD(, adj)	 adj_list;	/* adjacencies */
	struct event		 ev_connect;
	struct event		 keepalive_timer;
	struct event		 keepalive_timeout;
	struct event		 initdelay_timer;

	struct mapping_head	 mapping_list;
	struct mapping_head	 withdraw_list;
	struct mapping_head	 request_list;
	struct mapping_head	 release_list;
	struct mapping_head	 abortreq_list;

	struct in_addr		 addr;
	struct in_addr		 id;

	time_t			 uptime;
	u_int32_t		 peerid;	/* unique ID in DB */

	int			 fd;
	int			 state;
	int			 idtimer_cnt;
	u_int16_t		 keepalive;
};

struct mapping_entry {
	TAILQ_ENTRY(mapping_entry)	entry;
	struct map			map;
};

/* accept.c */
void	accept_init(void);
int	accept_add(int, void (*)(int, short, void *), void *);
void	accept_del(int);
void	accept_pause(void);
void	accept_unpause(void);

/* hello.c */
int	 send_hello(enum hello_type, struct iface *, struct tnbr *);
void	 recv_hello(struct iface *,  struct in_addr, char *, u_int16_t);

/* init.c */
void	 send_init(struct nbr *);
int	 recv_init(struct nbr *, char *, u_int16_t);

/* keepalive.c */
void	 send_keepalive(struct nbr *);
int	 recv_keepalive(struct nbr *, char *, u_int16_t);

/* notification.c */
void	 send_notification_nbr(struct nbr *, u_int32_t, u_int32_t, u_int32_t);
void	 send_notification(u_int32_t, struct tcp_conn *, u_int32_t,
	    u_int32_t);
int	 recv_notification(struct nbr *, char *, u_int16_t);

/* address.c */
void	 send_address(struct nbr *, struct if_addr *);
int	 recv_address(struct nbr *, char *, u_int16_t);
void	 send_address_withdraw(struct nbr *, struct if_addr *);

/* labelmapping.c */
#define PREFIX_SIZE(x)	(((x) + 7) / 8)
void	 send_labelmessage(struct nbr *, u_int16_t, struct mapping_head *);
int	 recv_labelmessage(struct nbr *, char *, u_int16_t, u_int16_t);

/* ldpe.c */
pid_t		 ldpe(struct ldpd_conf *, int[2], int[2], int[2]);
void		 ldpe_dispatch_main(int, short, void *);
void		 ldpe_dispatch_lde(int, short, void *);
int		 ldpe_imsg_compose_parent(int, pid_t, void *, u_int16_t);
int		 ldpe_imsg_compose_lde(int, u_int32_t, pid_t, void *,
		     u_int16_t);
u_int32_t	 ldpe_router_id(void);
void		 ldpe_fib_update(int);
void		 ldpe_iface_ctl(struct ctl_conn *, unsigned int);

/* interface.c */
int		 if_fsm(struct iface *, enum iface_event);

struct iface	*if_new(struct kif *);
void		 if_del(struct iface *);
void		 if_init(struct ldpd_conf *, struct iface *);
struct iface	*if_lookup(u_short);

struct ctl_iface	*if_to_ctl(struct iface *);

int	 if_join_group(struct iface *, struct in_addr *);
int	 if_leave_group(struct iface *, struct in_addr *);
int	 if_set_mcast(struct iface *);
int	 if_set_recvif(int, int);
void	 if_set_recvbuf(int);
int	 if_set_mcast_loop(int);
int	 if_set_mcast_ttl(int, u_int8_t);
int	 if_set_tos(int, int);
int	 if_set_reuse(int, int);

/* adjacency.c */
struct adj	*adj_new(struct nbr *, struct hello_source *, u_int16_t,
    struct in_addr);
void		 adj_del(struct adj *);
struct adj	*adj_find(struct nbr *, struct hello_source *);
void		 adj_start_itimer(struct adj *);
void		 adj_stop_itimer(struct adj *);
struct tnbr	*tnbr_new(struct ldpd_conf *, struct in_addr, int);
void		 tnbr_del(struct tnbr *);
void		 tnbr_init(struct ldpd_conf *, struct tnbr *);
struct tnbr	*tnbr_find(struct in_addr);

struct ctl_adj	*adj_to_ctl(struct adj *);
void		 ldpe_adj_ctl(struct ctl_conn *);

/* neighbor.c */
struct nbr	*nbr_new(struct in_addr, struct in_addr);
void		 nbr_del(struct nbr *);

struct nbr	*nbr_find_ldpid(u_int32_t);
struct nbr	*nbr_find_peerid(u_int32_t);

int	 nbr_fsm(struct nbr *, enum nbr_event);
int	 nbr_session_active_role(struct nbr *);

void	 nbr_ktimer(int, short, void *);
void	 nbr_start_ktimer(struct nbr *);
void	 nbr_stop_ktimer(struct nbr *);
void	 nbr_ktimeout(int, short, void *);
void	 nbr_start_ktimeout(struct nbr *);
void	 nbr_stop_ktimeout(struct nbr *);
void	 nbr_idtimer(int, short, void *);
void	 nbr_start_idtimer(struct nbr *);
void	 nbr_stop_idtimer(struct nbr *);
int	 nbr_pending_idtimer(struct nbr *);
int	 nbr_pending_connect(struct nbr *);

int	 nbr_establish_connection(struct nbr *);

void			 nbr_mapping_add(struct nbr *, struct mapping_head *,
			    struct map *);
struct mapping_entry	*nbr_mapping_find(struct nbr *, struct mapping_head *,
			    struct map *);
void			 nbr_mapping_del(struct nbr *, struct mapping_head *,
			    struct map *);
void			 mapping_list_clr(struct mapping_head *);

struct ctl_nbr	*nbr_to_ctl(struct nbr *);
void		 ldpe_nbr_ctl(struct ctl_conn *);

/* packet.c */
int	 gen_ldp_hdr(struct ibuf *, u_int16_t);
int	 gen_msg_tlv(struct ibuf *, u_int32_t, u_int16_t);
int	 send_packet(int, struct iface *, void *, size_t, struct sockaddr_in *);
void	 disc_recv_packet(int, short, void *);
void	 session_accept(int, short, void *);

struct tcp_conn *tcp_new(int, struct nbr *);
void		 tcp_close(struct tcp_conn *);

void	 session_read(int, short, void *);
void	 session_write(int, short, void *);
void	 session_close(struct nbr *);
void	 session_shutdown(struct nbr *, u_int32_t, u_int32_t, u_int32_t);

char	*pkt_ptr;	/* packet buffer */

#endif	/* _LDPE_H_ */
