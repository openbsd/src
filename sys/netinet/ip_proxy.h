/*       $OpenBSD: ip_proxy.h,v 1.3 1999/02/05 05:58:54 deraadt Exp $       */
/*
 * Copyright (C) 1997-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * $Id: ip_proxy.h,v 1.3 1999/02/05 05:58:54 deraadt Exp $
 */

#ifndef	__IP_PROXY_H__
#define	__IP_PROXY_H__

#ifndef SOLARIS
#define SOLARIS (defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#ifndef	APR_LABELLEN
#define	APR_LABELLEN	16
#endif
#define	AP_SESS_SIZE	53

struct	nat;
struct	ipnat;

typedef	struct	ap_tcp {
	u_short	apt_sport;	/* source port */
	u_short	apt_dport;	/* destination port */
	short	apt_sel[2];	/* {seq,ack}{off,min} set selector */
	short	apt_seqoff[2];	/* sequence # difference */
	tcp_seq	apt_seqmin[2];	/* don't change seq-off until after this */
	short	apt_ackoff[2];	/* sequence # difference */
	tcp_seq	apt_ackmin[2];	/* don't change seq-off until after this */
	u_char	apt_state[2];	/* connection state */
} ap_tcp_t;

typedef	struct	ap_udp {
	u_short	apu_sport;	/* source port */
	u_short	apu_dport;	/* destination port */
} ap_udp_t;

typedef	struct ap_session {
	struct	aproxy	*aps_apr;
	struct	in_addr	aps_src;	/* source IP# */
	struct	in_addr	aps_dst;	/* destination IP# */
	u_char	aps_p;		/* protocol */
	union {
		struct	ap_tcp	apu_tcp;
		struct	ap_udp	apu_udp;
	} aps_un;
	u_int	aps_hv;
	u_int	aps_flags;
	U_QUAD_T aps_bytes;	/* bytes sent */
	U_QUAD_T aps_pkts;	/* packets sent */
	void	*aps_nat;	/* pointer back to nat struct */
	void	*aps_data;	/* private data */
	int	aps_psiz;	/* size of private data */
	struct	ap_session	*aps_hnext;
	struct	ap_session	*aps_next;
} ap_session_t ;

#define	aps_sport	aps_un.apu_tcp.apt_sport
#define	aps_dport	aps_un.apu_tcp.apt_dport
#define	aps_sel		aps_un.apu_tcp.apt_sel
#define	aps_seqoff	aps_un.apu_tcp.apt_seqoff
#define	aps_seqmin	aps_un.apu_tcp.apt_seqmin
#define	aps_state	aps_un.apu_tcp.apt_state
#define	aps_ackoff	aps_un.apu_tcp.apt_ackoff
#define	aps_ackmin	aps_un.apu_tcp.apt_ackmin


typedef	struct	aproxy	{
	char	apr_label[APR_LABELLEN];	/* Proxy label # */
	u_char	apr_p;		/* protocol */
	int	apr_ref;	/* +1 per rule referencing it */
	int	apr_flags;
	int	(* apr_init) __P((fr_info_t *, ip_t *,
				   ap_session_t *, struct nat *));
	int	(* apr_inpkt) __P((fr_info_t *, ip_t *,
				   ap_session_t *, struct nat *));
	int	(* apr_outpkt) __P((fr_info_t *, ip_t *,
				    ap_session_t *, struct nat *));
} aproxy_t;

#define	APR_DELETE	1


extern	ap_session_t	*ap_sess_tab[AP_SESS_SIZE];
extern	ap_session_t	*ap_sess_list;
extern	aproxy_t	ap_proxies[];

extern	int	ap_ok __P((ip_t *, tcphdr_t *, struct ipnat *));
extern	void	ap_free __P((aproxy_t *));
extern	void	aps_free __P((ap_session_t *));
extern	int	ap_check __P((ip_t *, fr_info_t *, struct nat *));
extern	aproxy_t	*ap_match __P((u_char, char *));
extern	int	ap_fixseqack __P((fr_info_t *, ip_t *, ap_session_t *, int inc));
     

#endif /* __IP_PROXY_H__ */








