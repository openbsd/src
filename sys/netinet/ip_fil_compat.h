/*
 * (C)opyright 1993, 1994, 1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ip_fil_compat.h	1.8 1/14/96
 * $OpenBSD: ip_fil_compat.h,v 1.2 1996/10/08 07:33:26 niklas Exp $
 */

#ifndef	__IP_COMPAT_H_
#define	__IP_COMPAT_H__

#ifndef	SOLARIS
#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif
#define	IPMINLEN(i, h)	((i)->ip_len >= ((i)->ip_hl * 4 + sizeof(struct h)))

#ifndef	IP_OFFMASK
#define	IP_OFFMASK	0x1fff
#endif

#ifndef	MAX
#define	MAX(a,b)	(((a) > (b)) ? (a) : (b))
#endif

#ifdef _KERNEL
# if SOLARIS
#  define	MUTEX_ENTER(x)	mutex_enter(x)
#  define	MUTEX_EXIT(x)	mutex_exit(x)
#  define	MTOD(m,t)	(t)((m)->b_rptr)
#  define	IRCOPY(a,b,c)	copyin((a), (b), (c))
#  define	IWCOPY(a,b,c)	copyout((a), (b), (c))
# else
#  define	MUTEX_ENTER(x)	;
#  define	MUTEX_EXIT(x)	;
#  ifndef linux
#   define	MTOD(m,t)	mtod(m,t)
#   define	IRCOPY(a,b,c)	bcopy((a), (b), (c))
#   define	IWCOPY(a,b,c)	bcopy((a), (b), (c))
#  endif
# endif /* SOLARIS */

# ifdef sun
#  if defined(__svr4__) || defined(__SVR4)
#   define	GETUNIT(n)	get_unit((n))
#  else
#   include	<sys/kmem_alloc.h>
#   define	GETUNIT(n)	ifunit((n), IFNAMSIZ)
#  endif
# else
#  define	GETUNIT(n)	ifunit((n))
# endif /* sun */

# if defined(sun) && !defined(linux)
#  define	UIOMOVE(a,b,c,d)	uiomove(a,b,c,d)
#  define	SLEEP(id, n)	sleep((id), PZERO+1)
#  define	KFREE(x)	kmem_free((char *)(x), sizeof(*(x)))
#  if SOLARIS
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
#   define	SPLNET(x)	;
#   undef	SPLX
#   define	SPLX(x)		;
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
# endif /* BSD */
# if defined(NetBSD1_0) && (NetBSD1_0 > 1)
#  define	SPLNET(x)	x = splsoftnet()
# else
#  if !SOLARIS
#   define	SPLNET(x)	x = splnet()
#   define	SPLX(x)		(void) splx(x)
#  endif
# endif
#else
# define	MUTEX_ENTER(x)	;
# define	MUTEX_EXIT(x)	;
# define	SPLNET(x)	;
# define	SPLX(x)		;
# define	KMALLOC(x)	malloc(x)
# define	KFREE(x)	free(x)
# define	GETUNIT(x)	(x)
# define	IRCOPY(a,b,c)	bcopy((a), (b), (c))
# define	IWCOPY(a,b,c)	bcopy((a), (b), (c))
#endif /* KERNEL */

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
	__u16	uh_sport;
	__u16	uh_dport;
	__u16	uh_ulen;
	__u16	uh_sun;
} udphdr_t;

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

# define	SPLX(x)		(void)
# define	SPLNET(x)	(void)

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
typedef	struct	udphdr	udphdr_t;
typedef	struct	ip	ip_t;
#endif /* linux */

#endif	/* __IP_COMPAT_H__ */
