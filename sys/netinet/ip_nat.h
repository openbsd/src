/*
 * (C)opyright 1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ip_nat.h	1.5 2/4/96
 * $Id: ip_nat.h,v 1.4 1996/10/08 07:33:29 niklas Exp $
 */

#ifndef	__IP_NAT_H_
#define	__IP_NAT_H__

#ifndef SOLARIS
#define SOLARIS (defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#if defined(__STDC__) || defined(__GNUC__)
#define	SIOCADNAT	_IOW('r', 80, struct ipnat)
#define	SIOCRMNAT	_IOW('r', 81, struct ipnat)
#define	SIOCGNATS	_IOR('r', 82, struct natstat)
#define	SIOCGNATL	_IOWR('r', 83, struct natlookup)
#define SIOCGFRST	_IOR('r', 84, struct ipfrstat)
#define SIOCGIPST	_IOR('r', 85, struct ips_stat)
#else
#define	SIOCADNAT	_IOW(r, 80, struct ipnat)
#define	SIOCRMNAT	_IOW(r, 81, struct ipnat)
#define	SIOCGNATS	_IOR(r, 82, struct natstat)
#define	SIOCGNATL	_IOWR(r, 83, struct natlookup)
#define SIOCGFRST	_IOR(r, 84, struct ipfrstat)
#define SIOCGIPST	_IOR(r, 85, struct ips_stat)
#endif

#define	NAT_SIZE	367

typedef	struct	nat	{
	struct	nat	*nat_next;
	u_short	nat_use;
	short	nat_age;
	u_long	nat_sumd;
	struct	in_addr	nat_inip;
	struct	in_addr	nat_outip;
	struct	in_addr	nat_oip;	/* other ip */
	u_short	nat_oport;	/* other port */
	u_short	nat_inport;
	u_short	nat_outport;
} nat_t;

typedef	struct	ipnat	{
	struct	ipnat	*in_next;
	void	*in_ifp;
	u_short	in_flags;
	u_short	in_pnext;
	u_short	in_port[2];
	struct	in_addr	in_in[2];
	struct	in_addr	in_out[2];
	struct	in_addr	in_nextip;
	int	in_space;
	int	in_redir; /* 0 if it's a mapping, 1 if it's a hard redir */
	char	in_ifname[IFNAMSIZ];
} ipnat_t;

#define	in_pmin		in_port[0]	/* Also holds static redir port */
#define	in_pmax		in_port[1]
#define	in_nip		in_nextip.s_addr
#define	in_inip		in_in[0].s_addr
#define	in_inmsk	in_in[1].s_addr
#define	in_outip	in_out[0].s_addr
#define	in_outmsk	in_out[1].s_addr

#define	NAT_INBOUND	0
#define	NAT_OUTBOUND	1

#define	NAT_MAP		0
#define	NAT_REDIRECT	1

#define	IPN_CMPSIZ	(sizeof(struct in_addr) * 4 + sizeof(u_short) * 2)

typedef	struct	natlookup {
	struct	in_addr	nl_inip;
	struct	in_addr	nl_outip;
	u_short	nl_inport;
	u_short	nl_outport;
} natlookup_t;

typedef	struct	natstat	{
	u_long	ns_mapped[2];
	u_long	ns_added;
	u_long	ns_expire;
	u_long	ns_inuse;
	nat_t	***ns_table;
	ipnat_t	*ns_list;
} natstat_t;

#define	IPN_ANY		0
#define	IPN_TCP		1
#define	IPN_UDP		2
#define	IPN_TCPUDP	3

extern int nat_ioctl __P((caddr_t, int, int));
extern nat_t *nat_lookupoutip __P((ipnat_t *, ip_t *, tcphdr_t *));
extern nat_t *nat_lookupinip __P((struct in_addr, u_short));
extern nat_t *nat_lookupredir __P((natlookup_t *));
extern void ip_natout __P((ip_t *, int, fr_info_t *));
extern void ip_natin __P((ip_t *, int, fr_info_t *));
extern void ip_natunload __P((void));
extern void ip_natexpire __P((void));
#endif /* __IP_NAT_H__ */
