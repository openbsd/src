/*	$OpenBSD: ldpe.h,v 1.3 2010/02/20 21:05:00 michele Exp $ */

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

#define max(x,y) ((x) > (y) ? (x) : (y))

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

TAILQ_HEAD(ctl_conns, ctl_conn)	ctl_conns;

struct mapping_entry {
	TAILQ_ENTRY(mapping_entry)	entry;
	u_int32_t			label;
	u_int32_t			prefix;
	u_int8_t			prefixlen;
};

struct nbr {
	LIST_ENTRY(nbr)		 entry, hash;
	struct bufferevent	*bev;
	struct event		 inactivity_timer;
	struct event		 keepalive_timer;
	struct event		 keepalive_timeout;
	struct event		 initdelay_timer;

	struct mapping_head	 mapping_list;
	struct mapping_head	 withdraw_list;
	struct mapping_head	 request_list;
	struct mapping_head	 release_list;
	struct mapping_head	 abortreq_list;

	int			 fd;

	struct in_addr		 addr;
	struct in_addr		 id;

	struct iface		*iface;

	time_t			 uptime;

	u_int32_t		 peerid;	/* unique ID in DB */

	int			 state;

	u_int16_t		 lspace;
	u_int16_t		 holdtime;
	u_int16_t		 keepalive;

	u_int8_t		 priority;
	u_int8_t		 options;

	u_int8_t		 flags;
	u_int8_t		 hello_type;

};

/* hello.c */
int	 send_hello(struct iface *);
void	 recv_hello(struct iface *,  struct in_addr, char *, u_int16_t);

/* init.c */
int	 send_init(struct nbr *);
int	 recv_init(struct nbr *, char *, u_int16_t);

/* keepalive.c */
int	 send_keepalive(struct nbr *);
int	 recv_keepalive(struct nbr *, char *, u_int16_t);

/* notification.c */
int	 send_notification(u_int32_t, struct iface *, int, u_int32_t,
	    u_int32_t);
int	 send_notification_nbr(struct nbr *, u_int32_t, u_int32_t, u_int32_t);
int	 recv_notification(struct nbr *, char *, u_int16_t);

/* address.c */
int	 send_address(struct nbr *, struct iface *);
int	 recv_address(struct nbr *, char *, u_int16_t);
int	 send_address_withdraw(struct nbr *, struct iface *);
int	 recv_address_withdraw(struct nbr *, char *, u_int16_t);

/* labelmapping.c */
int	 send_labelmapping(struct nbr *);
int	 recv_labelmapping(struct nbr *, char *, u_int16_t);
int	 send_labelrequest(struct nbr *);
int	 recv_labelrequest(struct nbr *, char *, u_int16_t);
int	 send_labelwithdraw(struct nbr *);
int	 recv_labelwithdraw(struct nbr *, char *, u_int16_t);
int	 send_labelrelease(struct nbr *);
int	 recv_labelrelease(struct nbr *, char *, u_int16_t);
int	 send_labelabortreq(struct nbr *);
int	 recv_labelabortreq(struct nbr *, char *, u_int16_t);

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
void		 ldpe_nbr_ctl(struct ctl_conn *);

/* interface.c */
int		 if_fsm(struct iface *, enum iface_event);

struct iface	*if_new(struct kif *, struct kif_addr *);
void		 if_del(struct iface *);
void		 if_init(struct ldpd_conf *, struct iface *);

int		 if_act_start(struct iface *);
int		 if_act_reset(struct iface *);

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
int	 if_set_nonblock(int);

/* neighbor.c */
void		 nbr_init(u_int32_t);
struct nbr	*nbr_new(u_int32_t, u_int16_t, struct iface *, int);
void		 nbr_del(struct nbr *);

struct nbr	*nbr_find_ip(struct iface *, u_int32_t);
struct nbr	*nbr_find_ldpid(struct iface *, u_int32_t, u_int16_t);
struct nbr	*nbr_find_peerid(u_int32_t);

int	 nbr_fsm(struct nbr *, enum nbr_event);

void	 nbr_itimer(int, short, void *);
void	 nbr_start_itimer(struct nbr *);
void	 nbr_stop_itimer(struct nbr *);
void	 nbr_reset_itimer(struct nbr *);
void	 nbr_ktimer(int, short, void *);
void	 nbr_start_ktimer(struct nbr *);
void	 nbr_stop_ktimer(struct nbr *);
void	 nbr_reset_ktimer(struct nbr *);
void	 nbr_ktimeout(int, short, void *);
void	 nbr_start_ktimeout(struct nbr *);
void	 nbr_stop_ktimeout(struct nbr *);
void	 nbr_reset_ktimeout(struct nbr *);
void	 nbr_idtimer(int, short, void *);
void	 nbr_start_idtimer(struct nbr *);
void	 nbr_stop_idtimer(struct nbr *);
void	 nbr_reset_idtimer(struct nbr *);
int	 nbr_pending_idtimer(struct nbr *);

int	 nbr_act_session_establish(struct nbr *, int);
int	 nbr_close_connection(struct nbr *);

void			 nbr_mapping_add(struct nbr *, struct mapping_head *,
			    struct map *);
struct mapping_entry	*nbr_mapping_find(struct nbr *, struct mapping_head *,
			    struct map *);
void			 nbr_mapping_del(struct nbr *, struct mapping_head *,
			    struct map *);
void			 nbr_mapping_list_clr(struct nbr *,
			    struct mapping_head *);

struct ctl_nbr	*nbr_to_ctl(struct nbr *);

/* packet.c */
int	 gen_ldp_hdr(struct buf *, struct iface *, u_int16_t);
int	 gen_msg_tlv(struct buf *, u_int32_t, u_int16_t);
int	 send_packet(struct iface *, void *, size_t, struct sockaddr_in *);
void	 disc_recv_packet(int, short, void *);
void	 session_recv_packet(int, short, void *);

void	 session_read(struct bufferevent *, void *);
void	 session_error(struct bufferevent *, short, void *);
void	 session_close(struct nbr *);
void	 session_shutdown(struct nbr *, u_int32_t, u_int32_t, u_int32_t);

char	*pkt_ptr;	/* packet buffer */

#endif	/* _LDPE_H_ */
