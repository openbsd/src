/*
 * (C)opyright 1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ip_nat.h	1.3 1/12/96
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
#else
#define	SIOCADNAT	_IOW(r, 80, struct ipnat)
#define	SIOCRMNAT	_IOW(r, 81, struct ipnat)
#define	SIOCGNATS	_IOR(r, 82, struct natstat)
#endif

#define	NAT_SIZE	367

typedef	struct	nat	{
	struct	nat	*nat_next;
	u_short	nat_use;
	short	nat_age;
	u_long	nat_sumd;
	struct	in_addr	nat_inip;
	struct	in_addr	nat_outip;
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
	char	in_ifname[IFNAMSIZ];
} ipnat_t;

#define	in_pmin		in_port[0]
#define	in_pmax		in_port[1]
#define	in_nip		in_nextip.s_addr
#define	in_inip		in_in[0].s_addr
#define	in_inmsk	in_in[1].s_addr
#define	in_outip	in_out[0].s_addr
#define	in_outmsk	in_out[1].s_addr

#define	IPN_CMPSIZ	(sizeof(struct in_addr) * 4 + sizeof(u_short) * 2)

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

extern int nat_ioctl();
extern nat_t *nat_lookupoutip(), *nat_lookupinip();
extern void ip_natout(), ip_natin(), ip_natunload(), ip_natexpire();
#endif /* __IP_NAT_H__ */
