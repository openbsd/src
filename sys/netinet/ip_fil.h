/*
 * (C)opyright 1993, 1994, 1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ip_fil.h	1.23 11/11/95
 */

#ifndef	__IP_FIL_H_
#define	__IP_FIL_H__

#ifndef IPFILTER_LOG
#define IPFILTER_LOG 1
#endif

#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
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

#define	FI_SHORT	0x01
#define	FI_OPTIONS	0x02
#define	FI_FRAG		0x04
#define	FI_TCPUDP	0x08	/* TCP/UCP implied comparison involved */

typedef	struct	frentry {
	struct	frentry	*fr_next;
	struct	ifnet	*fr_ifa;
	u_int	fr_hits;

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
	u_short	fr_flags;	/* per-rule flags && options (see below) */
	char	fr_ifname[IFNAMSIZ];
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
#define	FR_BLOCK	0x0001
#define	FR_PASS		0x0002
#define	FR_OUTQUE	0x0004
#define	FR_INQUE	0x0008
#define	FR_LOGP		0x0010	/* Log-pass */
#define	FR_LOGB		0x0020	/* Log-fail */
#define	FR_LOG		0x0040	/* Log */
#define	FR_LOGBODY	0x0080	/* Log the body */
#define	FR_QUICK	0x0100
#define	FR_RETRST	0x0200
#define	FR_RETICMP	0x0400
#define	FR_INACTIVE	0x0800
#define	FR_NOMATCH	0x1000

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
	u_long	fr_pkl;		/* packets logged */
	u_long	fr_skip;	/* packets to be logged but buffer full */
	u_long	fr_ret;		/* packets for which a return is sent */
#if SOLARIS
	u_long	fr_bad;		/* bad IP packets to the filter */
	u_long	fr_notip;	/* packets passed through no on ip queue */
	u_long	fr_drop;	/* packets dropped - no info for them! */
#endif
} filterstats_t;

/*
 * recognized flags for SIOCGETFF and SIOCSETFF
 */
#define	FF_LOGPASS	1
#define	FF_LOGBLOCK	2

/*
 * For SIOCGETFS
 */
typedef	struct	friostat	{
	struct	filterstats	f_st[2];
	struct	frentry		*f_fin[2];
	struct	frentry		*f_fout[2];
	int	f_active;
} friostat_t;

typedef struct  optlist {
	u_short ol_val;
	int     ol_bit;
} optlist_t;

#ifdef	_KERNEL
extern struct frentry *filterin[], *filterout[];
extern struct filterstats frstats[];
#endif

typedef	struct ipl_ci	{
	u_long	sec;
	u_long	usec;
	u_char	hlen;
	u_char	plen;
	u_short	rule;
	u_long	flags:24;
	u_long	unit:8;
	u_char	ifname[4];
} ipl_ci_t;

#ifdef	_KERNEL

typedef	struct	ipfr	{
	struct	ipfr	*ipfr_next, *ipfr_prev;
	struct	in_addr	ipfr_src;
	struct	in_addr	ipfr_dst;
	u_short	ipfr_id;
	u_short	ipfr_age;
	u_char	ipfr_p;
	u_char	ipfr_tos;
	u_char	ipfr_pass;
} ipfr_t;

#define	IPFR_CMPSZ	(4 + 4 + 2 + 1 + 1)

# if defined(sun) && !defined(linux)
#  define	UIOMOVE(a,b,c,d)	uiomove(a,b,c,d)
#  define	SLEEP(id, n)	sleep((id), PZERO+1)
#  define	KFREE(x)	kmem_free((char *)(x), sizeof(*(x)))
#  if SOLARIS
#   ifdef	sparc
#    define	ntohs(x)	(x)
#    define	ntohl(x)	(x)
#    define	htons(x)	(x)
#    define	htonl(x)	(x)
#   endif
#   define	KMALLOC(x)	kmem_alloc((x), KM_SLEEP)
#   define	GET_MINOR(x)	getminor(x)
#  else
#   define	KMALLOC(x)	new_kmem_alloc((x), KMEM_SLEEP)
#  endif /* __svr4__ */
# endif /* sun && !linux */
# ifndef	GET_MINOR
#  define	GET_MINOR(x)	minor(x)
# endif
# if BSD >= 199306 || defined(__FreeBSD__)
#  include <vm/vm.h>
#  if !defined(__FreeBSD__)
#   include <vm/vm_extern.h>
#   include <sys/proc.h>
extern	vm_map_t	kmem_map;
#  else
#   include <vm/vm_kern.h>
#  endif /* __FreeBSD__ */
#  define	KMALLOC(x)	kmem_alloc(kmem_map, (x))
#  define	KFREE(x)	kmem_free(kmem_map, (vm_offset_t)(x), \
					  sizeof(*(x)))
#  define	UIOMOVE(a,b,c,d)	uiomove(a,b,d)
#  define	SLEEP(id, n)	tsleep((id), PPAUSE|PCATCH, n, 0)
# else
# endif /* BSD */
#endif /* _KERNEL */

#ifdef linux
# define	ICMP_UNREACH	ICMP_DEST_UNREACH
# define	ICMP_SOURCEQUENCH	ICMP_SOURCE_QUENCH
# define	ICMP_TIMXCEED	ICMP_TIME_EXCEEDED
# define	ICMP_PARAMPROB	ICMP_PARAMETERPROB
# define	icmp	icmphdr
# define	icmp_type	type
# define	icmp_code	code

# define	TH_FIN	0x01
# define	TH_SYN	0x02
# define	TH_RST	0x04
# define	TH_PUSH	0x08
# define	TH_ACK	0x10
# define	TH_URG	0x20

typedef	struct	{
	__u16	th_sport;
	__u16	th_dport;
	__u32	th_seq;
	__u32	th_ack;
	__u8	th_x;
	__u8	th_flags;
	__u16	th_win;
	__u16	th_sum;
	__u16	th_urp;
} tcphdr_t;

typedef	struct	{
# if defined(__i386__) || defined(__MIPSEL__) || defined(__alpha__) ||\
    defined(vax)
	__u8	ip_hl:4;
	__u8	ip_v:4;
# else
	__u8	ip_hl:4;
	__u8	ip_v:4;
# endif
	__u8	ip_tos;
	__u16	ip_len;
	__u16	ip_id;
	__u16	ip_off;
	__u8	ip_ttl;
	__u8	ip_p;
	__u16	ip_sum;
	__u32	ip_src;
	__u32	ip_dst;
} ip_t;

# define	SPLX(x)		;
# define	SPLNET(x)	;

# define	bcopy(a,b,c)	memmove(b,a,c)
# define	bcmp(a,b,c)	memcmp(a,b,c)

# define	UNITNAME(n)	dev_get((n))
# define	ifnet	device

# define	KMALLOC(x)	kmalloc((x), GFP_ATOMIC)
# define	KFREE(x)	kfree_s((x), sizeof(*(x)))
# define	IRCOPY(a,b,c)	{ \
				 error = verify_area(VERIFY_READ, \
						     (b) ,sizeof((b))); \
				 if (!error) \
					memcpy_fromfs((b), (a), (c)); \
				}
# define	IWCOPY(a,b,c)	{ \
				 error = verify_area(VERIFY_WRITE, \
						     (b) ,sizeof((b))); \
				 if (!error) \
					memcpy_tofs((b), (a), (c)); \
				}
#else

typedef	struct	tcphdr	tcphdr_t;
typedef	struct	ip	ip_t;

# if SOLARIS
#  define	MTOD(m,t)	(t)((m)->b_rptr)
#  define	IRCOPY(a,b,c)	copyin((a), (b), (c))
#  define	IWCOPY(a,b,c)	copyout((a), (b), (c))
#  ifdef	_KERNEL
typedef	struct	qif	{
	struct	qif	*qf_next;
	ill_t	*qf_ill;
	kmutex_t	qf_lock;
	void	*qf_iptr;
	void	*qf_optr;
	queue_t	*qf_in;
	queue_t	*qf_out;
	void	*qf_wqinfo;
	void	*qf_rqinfo;
	char	qf_name[8];
	int	(*qf_inp)();
	int	(*qf_outp)();
	/*
	 * in case the ILL has disappeared...
	 */
	int	qf_hl;	/* header length */
} qif_t;
#  endif /* _KERNEL */
# else
#  define	MTOD(m,t)	mtod(m,t)
#  define	IRCOPY(a,b,c)	bcopy((a), (b), (c))
#  define	IWCOPY(a,b,c)	bcopy((a), (b), (c))
# endif /* SOLARIS */
# ifdef	_KERNEL
#  if defined(NetBSD1_0) && (NetBSD1_0 > 1)
#   define	SPLNET(x)	x = splsoftnet()
#  else
#   if SOLARIS
#    define	SPLNET(x)	;
#   else
#    define	SPLNET(x)	x = splnet()
#   endif
#  endif
#  ifdef SPLX
#   undef	SPLX
#  endif
#  if SOLARIS
#   define	SPLX(x)		;
#  else
#   define	SPLX(x)		(void) splx(x)
#  endif
# else
#  define	SPLNET(x)	;
#  define	SPLX(x)		;
# endif /* KERNEL */

# ifdef sun
#  if !defined(__sysv__) && !defined(__SVR4)
#   define	GETUNIT(n)	ifunit((n), IFNAMSIZ)
#  endif
# else
#  define	GETUNIT(n)	ifunit((n))
# endif /* sun */
extern struct ifnet *ifunit();
#endif /* linux */

#define	IPMINLEN(i, h)	((i)->ip_len >= ((i)->ip_hl * 4 + sizeof(struct h)))

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

#endif	/* __IP_FIL_H__ */
