/*
 * (C)opyright 1993-1996 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ip_fil.h	1.35 6/5/96
 * $Id: ip_fil.h,v 1.4 1996/07/18 05:00:59 dm Exp $
 */

#ifndef	__IP_FIL_H__
#define	__IP_FIL_H__

#ifdef _KERNEL
#define IPFILTER_LOG
#endif /* _KERNEL */

#ifndef	SOLARIS
#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#if defined(KERNEL) && !defined(_KERNEL)
#define	_KERNEL
#endif
#if SOLARIS
# include <sys/ioccom.h>
# include <sys/sysmacros.h>
# ifdef	_KERNEL
#  include <inet/common.h>
/*
 * because Solaris 2 defines these in two places :-/
 */
#undef	IPOPT_EOL
#undef	IPOPT_NOP
#undef	IPOPT_LSRR
#undef	IPOPT_RR
#undef	IPOPT_SSRR
#  include <inet/ip.h>
# endif
#endif

#if defined(__STDC__) || defined(__GNUC__)
#define	SIOCADAFR	_IOW('r', 60, struct frentry)
#define	SIOCRMAFR	_IOW('r', 61, struct frentry)
#define	SIOCSETFF	_IOW('r', 62, u_int)
#define	SIOCGETFF	_IOR('r', 63, u_int)
#define	SIOCGETFS	_IOR('r', 64, struct friostat)
#define	SIOCIPFFL	_IOWR('r', 65, int)
#define	SIOCIPFFB	_IOR('r', 66, int)
#define	SIOCADIFR	_IOW('r', 67, struct frentry)
#define	SIOCRMIFR	_IOW('r', 68, struct frentry)
#define	SIOCSWAPA	_IOR('r', 69, u_int)
#define	SIOCINAFR	_IOW('r', 70, struct frentry)
#define	SIOCINIFR	_IOW('r', 71, struct frentry)
#define	SIOCFRENB	_IOW('r', 72, u_int)
#define	SIOCFRSYN	_IOW('r', 73, u_int)
#define	SIOCFRZST	_IOWR('r', 74, struct friostat)
#define	SIOCFLNAT	_IOWR('r', 75, int)
#define	SIOCCNATL	_IOWR('r', 76, int)
#define	SIOCZRLST	_IOWR('r', 77, struct frentry)
#else
#define	SIOCADAFR	_IOW(r, 60, struct frentry)
#define	SIOCRMAFR	_IOW(r, 61, struct frentry)
#define	SIOCSETFF	_IOW(r, 62, u_int)
#define	SIOCGETFF	_IOR(r, 63, u_int)
#define	SIOCGETFS	_IOR(r, 64, struct friostat)
#define	SIOCIPFFL	_IOWR(r, 65, int)
#define	SIOCIPFFB	_IOR(r, 66, int)
#define	SIOCADIFR	_IOW(r, 67, struct frentry)
#define	SIOCRMIFR	_IOW(r, 68, struct frentry)
#define	SIOCSWAPA	_IOR(r, 69, u_int)
#define	SIOCINAFR	_IOW(r, 70, struct frentry)
#define	SIOCINIFR	_IOW(r, 71, struct frentry)
#define SIOCFRENB	_IOW(r, 72, u_int)
#define	SIOCFRSYN	_IOW(r, 73, u_int)
#define	SIOCFRZST	_IOWR(r, 74, struct friostat)
#define	SIOCFLNAT	_IOWR(r, 75, int)
#define	SIOCCNATL	_IOWR(r, 76, int)
#define	SIOCZRLST	_IOWR(r, 77, struct frentry)
#endif
#define	SIOCADDFR	SIOCADAFR
#define	SIOCDELFR	SIOCRMAFR
#define	SIOCINSFR	SIOCINAFR

typedef	struct	fr_ip	{
	u_char	fi_v:4;
	u_char	fi_fl:4;
	u_char	fi_tos;
	u_char	fi_ttl;
	u_char	fi_p;
	struct	in_addr	fi_src;
	struct	in_addr	fi_dst;
	u_long	fi_optmsk;
	u_short	fi_secmsk;
	u_short	fi_auth;
} fr_ip_t;

#define	FI_OPTIONS	0x01
#define	FI_TCPUDP	0x02	/* TCP/UCP implied comparison involved */
#define	FI_FRAG		0x04
#define	FI_SHORT	0x08

typedef	struct	fr_info	{
	struct	fr_ip	fin_fi;
	void	*fin_ifp;
	u_short	fin_data[2];
	u_short	fin_out;
	u_char	fin_tcpf;
	u_char	fin_icode;
	u_short	fin_rule;
	u_short	fin_hlen;
	u_short	fin_dlen;
	char	*fin_dp;
	struct	frentry *fin_fr;
} fr_info_t;

#define	FI_CSIZE	(sizeof(struct fr_ip) + 11)

typedef	struct	frdest	{
	void	*fd_ifp;
	struct	in_addr	fd_ip;
	char	fd_ifname[IFNAMSIZ];
} frdest_t;

typedef	struct	frentry {
	struct	frentry	*fr_next;
	struct	ifnet	*fr_ifa;
	u_long	fr_hits;
	u_long	fr_bytes;	/* this is only incremented when a packet */
				/* stops matching on this rule */
	/*
	 * Fields after this may not change whilst in the kernel.
	 */
	struct	fr_ip	fr_ip;
	struct	fr_ip	fr_mip;

	u_char	fr_tcpfm;	/* tcp flags mask */
	u_char	fr_tcpf;	/* tcp flags */

	u_short	fr_icmpm;	/* data for ICMP packets (mask) */
	u_short	fr_icmp;

	u_char	fr_scmp;	/* data for port comparisons */
	u_char	fr_dcmp;
	u_short	fr_dport;
	u_short	fr_sport;
	u_short	fr_stop;	/* top port for <> and >< */
	u_short	fr_dtop;	/* top port for <> and >< */
	u_long	fr_flags;	/* per-rule flags && options (see below) */
	int	(*fr_func)();	/* call this function */
	char	fr_icode;	/* return ICMP code */
	char	fr_ifname[IFNAMSIZ];
	struct	frdest	fr_tif;	/* "to" interface */
	struct	frdest	fr_dif;	/* duplicate packet interfaces */
} frentry_t;

#define	fr_proto	fr_ip.fi_p
#define	fr_ttl		fr_ip.fi_ttl
#define	fr_tos		fr_ip.fi_tos
#define	fr_dst		fr_ip.fi_dst
#define	fr_src		fr_ip.fi_src
#define	fr_dmsk		fr_mip.fi_dst
#define	fr_smsk		fr_mip.fi_src

#ifndef	offsetof
#define	offsetof(t,m)	(int)((&((t *)0L)->m))
#endif
#define	FR_CMPSIZ	(sizeof(struct frentry) - offsetof(frentry_t, fr_ip))

/*
 * fr_flags
*/
#define	FR_BLOCK	0x00001
#define	FR_PASS		0x00002
#define	FR_OUTQUE	0x00004
#define	FR_INQUE	0x00008
#define	FR_LOG		0x00010	/* Log */
#define	FR_LOGB		0x00011	/* Log-fail */
#define	FR_LOGP		0x00012	/* Log-pass */
#define	FR_LOGBODY	0x00020	/* Log the body */
#define	FR_LOGFIRST	0x00040
#define	FR_RETRST	0x00080
#define	FR_RETICMP	0x00100
#define	FR_NOMATCH	0x00200
#define	FR_ACCOUNT	0x00400	/* count packet bytes */
#define	FR_KEEPFRAG	0x00800
#define	FR_KEEPSTATE	0x01000
#define	FR_INACTIVE	0x02000
#define	FR_QUICK	0x04000
#define	FR_FASTROUTE	0x08000
#define	FR_CALLNOW	0x10000
#define	FR_DUP		0x20000

#define	FR_LOGMASK	(FR_LOG|FR_LOGP|FR_LOGB)
/*
 * recognized flags for SIOCGETFF and SIOCSETFF
 */
#define	FF_LOGPASS	0x100000
#define	FF_LOGBLOCK	0x200000
#define	FF_LOGNOMATCH	0x400000
#define	FF_LOGGING	(FF_LOGPASS|FF_LOGBLOCK|FF_LOGNOMATCH)

#define	FR_NONE 0
#define	FR_EQUAL 1
#define	FR_NEQUAL 2
#define FR_LESST 3
#define FR_GREATERT 4
#define FR_LESSTE 5
#define FR_GREATERTE 6
#define	FR_OUTRANGE 7
#define	FR_INRANGE 8

typedef	struct	filterstats {
	u_long	fr_pass;	/* packets allowed */
	u_long	fr_block;	/* packets denied */
	u_long	fr_nom;		/* packets which don't match any rule */
	u_long	fr_ppkl;	/* packets allowed and logged */
	u_long	fr_bpkl;	/* packets denied and logged */
	u_long	fr_npkl;	/* packets unmatched and logged */
	u_long	fr_pkl;		/* packets logged */
	u_long	fr_skip;	/* packets to be logged but buffer full */
	u_long	fr_ret;		/* packets for which a return is sent */
	u_long	fr_acct;	/* packets for which counting was performed */
	u_long	fr_bnfr;	/* bad attempts to allocate fragment state */
	u_long	fr_nfr;		/* new fragment state kept */
	u_long	fr_cfr;		/* add new fragment state but complete pkt */
	u_long	fr_bads;	/* bad attempts to allocate packet state */
	u_long	fr_ads;		/* new packet state kept */
	u_long	fr_chit;	/* cached hit */
#if SOLARIS
	u_long	fr_bad;		/* bad IP packets to the filter */
	u_long	fr_notip;	/* packets passed through no on ip queue */
	u_long	fr_drop;	/* packets dropped - no info for them! */
#endif
} filterstats_t;

/*
 * For SIOCGETFS
 */
typedef	struct	friostat	{
	struct	filterstats	f_st[2];
	struct	frentry		*f_fin[2];
	struct	frentry		*f_fout[2];
	struct	frentry		*f_acctin[2];
	struct	frentry		*f_acctout[2];
	int	f_active;
} friostat_t;

typedef struct  optlist {
	u_short ol_val;
	int     ol_bit;
} optlist_t;

typedef	struct ipl_ci	{
	u_long	sec;
	u_long	usec;
	u_char	hlen;
	u_char	plen;
	u_short	rule;
	u_long	flags:24;			/* XXX FIXME do we care about the extra bytes? */
#if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606))
	u_long	filler:8;			/* XXX FIXME do we care? */
	u_char	ifname[IFNAMSIZ];
#else
	u_long	unit:8;
	u_char	ifname[4];
#endif
} ipl_ci_t;


#ifndef	ICMP_UNREACH_FILTER
#define	ICMP_UNREACH_FILTER	13
#endif
/*
 * Security Options for Intenet Protocol (IPSO) as defined in RFC 1108.
 *
 * Basic Option
 *
 * 00000001   -   (Reserved 4)
 * 00111101   -   Top Secret
 * 01011010   -   Secret
 * 10010110   -   Confidential
 * 01100110   -   (Reserved 3)
 * 11001100   -   (Reserved 2)
 * 10101011   -   Unclassified
 * 11110001   -   (Reserved 1)
 */
#define	IPSO_CLASS_RES4		0x01
#define	IPSO_CLASS_TOPS		0x3d
#define	IPSO_CLASS_SECR		0x5a
#define	IPSO_CLASS_CONF		0x96
#define	IPSO_CLASS_RES3		0x66
#define	IPSO_CLASS_RES2		0xcc
#define	IPSO_CLASS_UNCL		0xab
#define	IPSO_CLASS_RES1		0xf1

#define	IPSO_AUTH_GENSER	0x80
#define	IPSO_AUTH_ESI		0x40
#define	IPSO_AUTH_SCI		0x20
#define	IPSO_AUTH_NSA		0x10
#define	IPSO_AUTH_DOE		0x08
#define	IPSO_AUTH_UN		0x06
#define	IPSO_AUTH_FTE		0x01

/*#define	IPOPT_RR	7 */
#define	IPOPT_ZSU	10	/* ZSU */
#define	IPOPT_MTUP	11	/* MTUP */
#define	IPOPT_MTUR	12	/* MTUR */
#define	IPOPT_ENCODE	15	/* ENCODE */
/*#define	IPOPT_TS	68 */
#define	IPOPT_TR	82	/* TR */
/*#define	IPOPT_SECURITY	130 */
/*#define	IPOPT_LSRR	131 */
#define	IPOPT_E_SEC	133	/* E-SEC */
#define	IPOPT_CIPSO	134	/* CIPSO */
/*#define	IPOPT_SATID	136 */
#ifndef	IPOPT_SID
# define	IPOPT_SID	IPOPT_SATID
#endif
/*#define	IPOPT_SSRR	137 */
#define	IPOPT_ADDEXT	147	/* ADDEXT */
#define	IPOPT_VISA	142	/* VISA */
#define	IPOPT_IMITD	144	/* IMITD */
#define	IPOPT_EIP	145	/* EIP */
#define	IPOPT_FINN	205	/* FINN */

#define	IPMINLEN(i, h)	((i)->ip_len >= ((i)->ip_hl * 4 + sizeof(struct h)))

extern	int	fr_check();
extern	fr_info_t	frcache[];

#ifdef _KERNEL

extern struct frentry *ipfilter[2][2], *ipacct[2][2];
extern struct filterstats frstats[];
# if	SOLARIS
extern	int	ipfsync();
# endif
#endif /* _KERNEL */
#endif	/* __IP_FIL_H__ */
