/*	$OpenBSD: ldpe.h,v 1.48 2016/05/23 18:33:56 renato Exp $ */

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
#include <net/pfkeyv2.h>

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
	uint16_t		 holdtime;
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
	RB_ENTRY(nbr)		 id_tree, addr_tree, pid_tree;
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

	struct in_addr		 laddr;		/* local address */
	struct in_addr		 raddr;		/* remote address */
	struct in_addr		 id;		/* lsr id */

	time_t			 uptime;
	uint32_t		 peerid;	/* unique ID in DB */

	int			 fd;
	int			 state;
	int			 idtimer_cnt;
	uint16_t		 keepalive;
	uint16_t		 max_pdu_len;

	struct {
		uint8_t			established;
		uint32_t		spi_in;
		uint32_t		spi_out;
		enum auth_method	method;
		char			md5key[TCP_MD5_KEY_LEN];
	} auth;
};

struct pending_conn {
	TAILQ_ENTRY(pending_conn)	 entry;
	int				 fd;
	struct in_addr			 addr;
	struct event			 ev_timeout;
};
#define PENDING_CONN_TIMEOUT	5

struct mapping_entry {
	TAILQ_ENTRY(mapping_entry)	entry;
	struct map			map;
};

struct ldpd_sysdep {
	uint8_t		no_pfkey;
	uint8_t		no_md5sig;
};

/* accept.c */
void	accept_init(void);
int	accept_add(int, void (*)(int, short, void *), void *);
void	accept_del(int);
void	accept_pause(void);
void	accept_unpause(void);

/* hello.c */
int	 send_hello(enum hello_type, struct iface *, struct tnbr *);
void	 recv_hello(struct in_addr, struct ldp_msg *, struct in_addr,
	    struct iface *, int, char *, uint16_t);

/* init.c */
void	 send_init(struct nbr *);
int	 recv_init(struct nbr *, char *, uint16_t);

/* keepalive.c */
void	 send_keepalive(struct nbr *);
int	 recv_keepalive(struct nbr *, char *, uint16_t);

/* notification.c */
void	 send_notification_nbr(struct nbr *, uint32_t, uint32_t, uint32_t);
void	 send_notification(uint32_t, struct tcp_conn *, uint32_t,
	    uint32_t);
void	 send_notification_full(struct tcp_conn *, struct notify_msg *);
int	 recv_notification(struct nbr *, char *, uint16_t);

/* address.c */
void	 send_address(struct nbr *, struct if_addr *, int);
int	 recv_address(struct nbr *, char *, uint16_t);

/* labelmapping.c */
#define PREFIX_SIZE(x)	(((x) + 7) / 8)
void	 send_labelmessage(struct nbr *, uint16_t, struct mapping_head *);
int	 recv_labelmessage(struct nbr *, char *, uint16_t, uint16_t);
void	 gen_fec_tlv(struct ibuf *, struct map *);
void	 gen_pw_status_tlv(struct ibuf *, uint32_t);
int	 tlv_decode_fec_elm(struct nbr *, struct ldp_msg *, char *,
	    uint16_t, struct map *);

/* ldpe.c */
pid_t		 ldpe(struct ldpd_conf *, int[2], int[2], int[2]);
int		 ldpe_imsg_compose_parent(int, pid_t, void *, uint16_t);
int		 ldpe_imsg_compose_lde(int, uint32_t, pid_t, void *,
		    uint16_t);
void		 ldpe_dispatch_main(int, short, void *);
void		 ldpe_dispatch_lde(int, short, void *);
void		 ldpe_dispatch_pfkey(int, short, void *);
void		 ldpe_setup_sockets(int, int, int);
void		 ldpe_close_sockets(void);
void		 ldpe_iface_ctl(struct ctl_conn *, unsigned int);
void		 ldpe_adj_ctl(struct ctl_conn *);
void		 ldpe_nbr_ctl(struct ctl_conn *);
void		 mapping_list_add(struct mapping_head *, struct map *);
void		 mapping_list_clr(struct mapping_head *);

/* interface.c */
int		 if_start(struct iface *);
int		 if_reset(struct iface *);
int		 if_update(struct iface *);
void		 if_update_all(void);

struct iface	*if_new(struct kif *);
void		 if_del(struct iface *);
struct iface	*if_lookup(struct ldpd_conf *, unsigned short);
struct if_addr	*if_addr_new(struct kaddr *);
struct if_addr	*if_addr_lookup(struct if_addr_head *, struct kaddr *);
void		 if_addr_add(struct kaddr *);
void		 if_addr_del(struct kaddr *);

struct ctl_iface	*if_to_ctl(struct iface *);

int	 if_join_group(struct iface *, struct in_addr *);
int	 if_leave_group(struct iface *, struct in_addr *);

/* adjacency.c */
struct adj	*adj_new(struct nbr *, struct hello_source *, struct in_addr);
void		 adj_del(struct adj *);
struct adj	*adj_find(struct nbr *, struct hello_source *);
void		 adj_start_itimer(struct adj *);
void		 adj_stop_itimer(struct adj *);
struct tnbr	*tnbr_new(struct ldpd_conf *, struct in_addr);
void		 tnbr_del(struct tnbr *);
struct tnbr	*tnbr_find(struct ldpd_conf *, struct in_addr);
struct tnbr	*tnbr_check(struct tnbr *);
void		 tnbr_update(struct tnbr *);
void		 tnbr_update_all(void);

struct ctl_adj	*adj_to_ctl(struct adj *);

/* neighbor.c */
struct nbr	*nbr_new(struct in_addr, struct in_addr);
void		 nbr_del(struct nbr *);
void		 nbr_update_peerid(struct nbr *);

struct nbr	*nbr_find_ldpid(uint32_t);
struct nbr	*nbr_find_addr(struct in_addr);
struct nbr	*nbr_find_peerid(uint32_t);

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

uint16_t		 nbr_get_keepalive(struct in_addr);
struct nbr_params	*nbr_params_new(struct in_addr);
struct nbr_params	*nbr_params_find(struct ldpd_conf *, struct in_addr);

struct ctl_nbr	*nbr_to_ctl(struct nbr *);

extern struct nbr_id_head	nbrs_by_id;
RB_PROTOTYPE(nbr_id_head, nbr, id_tree, nbr_id_compare)
extern struct nbr_addr_head	nbrs_by_addr;
RB_PROTOTYPE(nbr_addr_head, nbr, addr_tree, nbr_addr_compare)
extern struct nbr_pid_head	nbrs_by_pid;
RB_PROTOTYPE(nbr_pid_head, nbr, pid_tree, nbr_pid_compare)

/* packet.c */
int	 gen_ldp_hdr(struct ibuf *, uint16_t);
int	 gen_msg_hdr(struct ibuf *, uint32_t, uint16_t);
int	 send_packet(int, struct iface *, void *, size_t, struct sockaddr_in *);
void	 disc_recv_packet(int, short, void *);
void	 session_accept(int, short, void *);
void	 session_accept_nbr(struct nbr *, int);
void	 session_read(int, short, void *);
void	 session_write(int, short, void *);
void	 session_close(struct nbr *);
void	 session_shutdown(struct nbr *, uint32_t, uint32_t, uint32_t);

struct tcp_conn		*tcp_new(int, struct nbr *);
void			 tcp_close(struct tcp_conn *);
struct pending_conn	*pending_conn_new(int, struct in_addr);
void			 pending_conn_del(struct pending_conn *);
struct pending_conn	*pending_conn_find(struct in_addr);
void			 pending_conn_timeout(int, short, void *);

char	*pkt_ptr;	/* packet buffer */

/* pfkey.c */
int	pfkey_read(int, struct sadb_msg *);
int	pfkey_establish(struct nbr *, struct nbr_params *);
int	pfkey_remove(struct nbr *);
int	pfkey_init(struct ldpd_sysdep *);

/* l2vpn.c */
void	ldpe_l2vpn_init(struct l2vpn *);
void	ldpe_l2vpn_exit(struct l2vpn *);
void	ldpe_l2vpn_pw_init(struct l2vpn_pw *);
void	ldpe_l2vpn_pw_exit(struct l2vpn_pw *);

#endif	/* _LDPE_H_ */
