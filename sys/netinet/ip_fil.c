/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#ifndef	lint
static	char	sccsid[] = "@(#)ip_fil.c	2.37 4/10/96 (C) 1993-1995 Darren Reed";
#endif

#ifndef	linux
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/dir.h>
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#ifdef sun
#include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include <syslog.h>
#endif
#include "ip_fil.h"
#include "ip_frag.h"
#include "ip_nat.h"
#include "ip_state.h"
#ifndef	MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif

extern	fr_flags, fr_active;
extern	struct	protosw	inetsw[];
extern	int	(*fr_checkp)();
#if	BSD < 199306
extern	int	ipfr_slowtimer();
static	int	(*fr_saveslowtimo)();
extern	int	tcp_ttl;
#else
extern	void	ipfr_slowtimer();
static	void	(*fr_saveslowtimo)();
#endif

int	ipl_inited = 0;
int	ipl_unreach = ICMP_UNREACH_FILTER;
int	send_reset();

#ifdef	IPFILTER_LOG
#define	LOGSIZE	8192
int	ipllog();
static	char	iplbuf[LOGSIZE];
static	caddr_t	iplh = iplbuf, iplt = iplbuf;
static	int	iplused = 0;
#endif /* IPFILTER_LOG */
static	void	frflush();
static	int	frrequest();
static	int	iplbusy = 0;
static	int	(*fr_savep)();

#if _BSDI_VERSION >= 199510
#include <sys/device.h>
#include <sys/conf.h>

int	iplioctl __P((dev_t, int, caddr_t, int, struct proc *));
int	iplopen __P((dev_t, int, int, struct proc *));
int	iplclose __P((dev_t, int, int, struct proc *));
#ifdef IPFILTER_LOG
int	iplread __P((dev_t, struct uio *, int));
#else
#define iplread noread
#endif
int	iplioctl __P((dev_t, int, caddr_t, int, struct proc *));

struct cfdriver iplcd = {
	NULL, "ipl", NULL, NULL, DV_DULL, 0
};

struct devsw iplsw = {
	&iplcd,
	iplopen, iplclose, iplread, nowrite, iplioctl, noselect, nommap,
	nostrat, nodump, nopsize, 0,
	nostop
};
#endif

#ifdef	IPFILTER_LKM
int iplidentify(s)
char *s;
{
	if (strcmp(s, "ipl") == 0)
		return 1;
	return 0;
}
#endif /* IPFILTER_LKM */


int iplattach()
{
	int s;

	SPLNET(s);
	if (ipl_inited || (fr_checkp == fr_check)) {
		printf("ipl: already initialized (%d)\n", iplbusy);
		SPLX(s);
		return EBUSY;
	}
	ipl_inited = 1;
	fr_savep = fr_checkp;
	fr_checkp = fr_check;
	fr_saveslowtimo = inetsw[0].pr_slowtimo;
	inetsw[0].pr_slowtimo = ipfr_slowtimer;
	SPLX(s);
	return 0;
}


int ipldetach()
{
	int s, i = FR_INQUE|FR_OUTQUE;

	if (iplbusy > 1) {
		printf("iplbusy: %d\n", iplbusy);
		return EBUSY;
	}
	SPLNET(s);
	if (!ipl_inited)
	{
		printf("ipl: not initialized\n");
		SPLX(s);
		return EBUSY;
	}

	fr_checkp = fr_savep;
	inetsw[0].pr_slowtimo = fr_saveslowtimo;
	frflush((caddr_t)&i);
	ipl_inited = 0;

	ipfr_unload();
	ip_natunload();
	fr_stateunload();

	SPLX(s);
	return 0;
}


static	void	frzerostats(data)
caddr_t	data;
{
	struct	friostat	fio;

	bcopy((char *)frstats, (char *)fio.f_st,
		sizeof(struct filterstats) * 2);
	fio.f_fin[0] = ipfilter[0][0];
	fio.f_fin[1] = ipfilter[0][1];
	fio.f_fout[0] = ipfilter[1][0];
	fio.f_fout[1] = ipfilter[1][1];
	fio.f_acctin[0] = ipacct[0][0];
	fio.f_acctin[1] = ipacct[0][1];
	fio.f_acctout[0] = ipacct[1][0];
	fio.f_acctout[1] = ipacct[1][1];
	fio.f_active = fr_active;
	IWCOPY((caddr_t)&fio, data, sizeof(fio));
	bzero((char *)frstats, sizeof(*frstats));
}


static void frflush(data)
caddr_t data;
{
	struct frentry *f, **fp;
	int flags = *(int *)data, flushed = 0, set = fr_active;

	bzero((char *)frcache, sizeof(frcache[0]) * 2);

	if (flags & FR_INACTIVE)
		set = 1 - set;
	if (flags & FR_OUTQUE) {
		for (fp = &ipfilter[1][set]; (f = *fp); ) {
			*fp = f->fr_next;
			KFREE(f);
			flushed++;
		}
		for (fp = &ipacct[1][set]; (f = *fp); ) {
			*fp = f->fr_next;
			KFREE(f);
			flushed++;
		}
	}
	if (flags & FR_INQUE) {
		for (fp = &ipfilter[0][set]; (f = *fp); ) {
			*fp = f->fr_next;
			KFREE(f);
			flushed++;
		}
		for (fp = &ipacct[0][set]; (f = *fp); ) {
			*fp = f->fr_next;
			KFREE(f);
			flushed++;
		}
	}
	*(int *)data = flushed;
}


/*
 * Filter ioctl interface.
 */
int iplioctl(dev, cmd, data, mode
#if _BSDI_VERSION >= 199510
, p)
struct proc *p;
#else
)
#endif
dev_t dev;
int cmd;
caddr_t data;
int mode;
{
	int error = 0, s, unit, used;

	unit = minor(dev);
	if (unit != 0)
		return ENXIO;

	SPLNET(s);
	switch (cmd) {
	case FIONREAD :
		*(int *)data = iplused;
		break;
#ifndef	IPFILTER_LKM
	case SIOCFRENB :
	{
		u_int	enable;

		IRCOPY(data, (caddr_t)&enable, sizeof(enable));
		if (enable)
			error = iplattach();
		else
			error = ipldetach();
		break;
	}
#endif
	case SIOCSETFF :
		IRCOPY(data, (caddr_t)&fr_flags, sizeof(fr_flags));
		break;
	case SIOCGETFF :
		IWCOPY((caddr_t)&fr_flags, data, sizeof(fr_flags));
		break;
	case SIOCINAFR :
	case SIOCRMAFR :
	case SIOCADAFR :
		error = frrequest(cmd, (struct frentry *)data, fr_active);
		break;
	case SIOCINIFR :
	case SIOCRMIFR :
	case SIOCADIFR :
		error = frrequest(cmd, (struct frentry *)data, 1-fr_active);
		break;
	case SIOCSWAPA :
		bzero((char *)frcache, sizeof(frcache[0]) * 2);
		*(u_int *)data = fr_active;
		fr_active = 1 - fr_active;
		break;
	case SIOCGETFS :
	{
		struct	friostat	fio;

		bcopy((char *)frstats, (char *)fio.f_st,
			sizeof(struct filterstats) * 2);
		fio.f_fin[0] = ipfilter[0][0];
		fio.f_fin[1] = ipfilter[0][1];
		fio.f_fout[0] = ipfilter[1][0];
		fio.f_fout[1] = ipfilter[1][1];
		fio.f_acctin[0] = ipacct[0][0];
		fio.f_acctin[1] = ipacct[0][1];
		fio.f_acctout[0] = ipacct[1][0];
		fio.f_acctout[1] = ipacct[1][1];
		fio.f_active = fr_active;
		IWCOPY((caddr_t)&fio, data, sizeof(fio));
		break;
	}
	case	SIOCFRZST :
		frzerostats(data);
		break;
	case	SIOCIPFFL :
		frflush(data);
		break;
#ifdef	IPFILTER_LOG
	case	SIOCIPFFB :
		*(int *)data = iplused;
		iplh = iplt = iplbuf;
		iplused = 0;
		break;
#endif /* IPFILTER_LOG */
	case SIOCADNAT :
	case SIOCRMNAT :
	case SIOCGNATS :
	case SIOCGNATL :
		error = nat_ioctl(data, cmd);
		break;
	case SIOCGFRST :
		IWCOPY((caddr_t)ipfr_fragstats(), data, sizeof(ipfrstat_t));
		break;
	case SIOCGIPST :
		IWCOPY((caddr_t)fr_statetstats(), data, sizeof(ips_stat_t));
		break;
	default :
		error = -EINVAL;
		break;
	}
	SPLX(s);
	return error;
}


static int frrequest(req, fp, set)
int req, set;
register struct frentry *fp;
{
	register struct frentry *f, **fprev;
	register struct frentry **ftail;
	struct frentry frd;
	int error = 0, in;

	bzero((char *)frcache, sizeof(frcache[0]) * 2);

	in = (fp->fr_flags & FR_INQUE) ? 0 : 1;
	if (fp->fr_flags & FR_ACCOUNT) {
		ftail = fprev = &ipacct[in][set];
	} else if (fp->fr_flags & (FR_OUTQUE|FR_INQUE))
		ftail = fprev = &ipfilter[in][set];
	else
		return ESRCH;

	IRCOPY((char *)fp, (char *)&frd, sizeof(frd));
	fp = &frd;
	if (*fp->fr_ifname) {
		fp->fr_ifa = GETUNIT(fp->fr_ifname);
		if (!fp->fr_ifa)
			fp->fr_ifa = (struct ifnet *)-1;
	}
	/*
	 * Look for a matching filter rule, but don't include the next or
	 * interface pointer in the comparison (fr_next, fr_ifa).
	 */
	for (; (f = *ftail); ftail = &f->fr_next)
		if (bcmp((char *)&f->fr_ip, (char *)&fp->fr_ip,
			 FR_CMPSIZ) == 0)
			break;
	if (!f) {
		ftail = fprev;
		if (req != SIOCINAFR && req != SIOCINIFR)
			while ((f = *ftail))
				ftail = &f->fr_next;
		else if (fp->fr_hits)
			while (--fp->fr_hits && (f = *ftail))
				ftail = &f->fr_next;
		f = NULL;
	}

	if (req == SIOCDELFR || req == SIOCRMIFR) {
		if (!f)
			error = ESRCH;
		else {
			*ftail = f->fr_next;
			(void) KFREE(f);
		}
	} else {
		if (f)
			error = EEXIST;
		else {
			if ((f = (struct frentry *)KMALLOC(sizeof(*f)))) {
				bcopy((char *)fp, (char *)f, sizeof(*f));
				f->fr_hits = 0;
				f->fr_next = *ftail;
				*ftail = f;
			} else
				error = ENOMEM;
		}
	}
	return (error);
}


#if !defined(linux)
/*
 * routines below for saving IP headers to buffer
 */
int iplopen(dev, flags
#if _BSDI_VERSION >= 199510
, devtype, p)
int devtype;
struct proc *p;
#else
)
#endif
dev_t dev;
int flags;
{
	u_int min = minor(dev);

	if ((flags & FWRITE) || min)
		min = ENXIO;
	else
		iplbusy++;
	return min;
}


int iplclose(dev, flags
#if _BSDI_VERSION >= 199510
, devtype, p)
int devtype;
struct proc *p;
#else
)
#endif
dev_t dev;
int flags;
{
	u_int	min = minor(dev);

	if (min)
		min = ENXIO;
	else if (iplbusy > 0)
		iplbusy--;
	return min;
}

# ifdef	IPFILTER_LOG
/*
 * iplread/ipllog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
#  if BSD >= 199306
int iplread(dev, uio, ioflag)
int ioflag;
#  else
int iplread(dev, uio)
#  endif
dev_t dev;
register struct uio *uio;
{
	register int ret, s;
	register size_t sz, sx;
	int error;

	if (!uio->uio_resid)
		return 0;
	while (!iplused) {
		error = SLEEP(iplbuf, "ipl sleep");
		if (error)
			return error;
	}

	SPLNET(s);

	sx = sz = MIN(uio->uio_resid, iplused);
	if (iplh < iplt)
		sz = MIN(sz, LOGSIZE - (iplt - iplbuf));
	sx -= sz;

#  if BSD >= 199306 || defined(__FreeBSD__)
	uio->uio_rw = UIO_READ;
#  endif
	if (!(ret = UIOMOVE(iplt, sz, UIO_READ, uio))) {
		iplt += sz;
		iplused -= sz;
		if ((iplh < iplt) && (iplt == iplbuf + LOGSIZE))
			iplt = iplbuf;

		if (sx && !(ret = UIOMOVE(iplt, sx, UIO_READ, uio))) {
			iplt += sx;
			iplused -= sx;
			if ((iplh < iplt) && (iplt == iplbuf + LOGSIZE))
				iplt = iplbuf;
		}
		if (!iplused)	/* minimise wrapping around the end */
			iplh = iplt = iplbuf;
	}
	SPLX(s);
	return ret;
}
# endif /* IPFILTER_LOG */
#endif /* linux */


#ifdef	IPFILTER_LOG
int ipllog(flags, ip, fin, m)
u_int flags;
ip_t *ip;
register fr_info_t *fin;
struct mbuf *m;
{
	struct ipl_ci iplci;
	register size_t tail = 0;
	register int len, mlen, hlen;
	struct ifnet *ifp = fin->fin_ifp;

	hlen = fin->fin_hlen;
	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
		hlen += MIN(sizeof(tcphdr_t), fin->fin_dlen);
	else if (ip->ip_p == IPPROTO_ICMP) {
		struct	icmp	*icmp = (struct icmp *)((char *)ip + hlen);

		switch (icmp->icmp_type) {
		case ICMP_UNREACH :
		case ICMP_SOURCEQUENCH :
		case ICMP_REDIRECT :
		case ICMP_TIMXCEED :
		case ICMP_PARAMPROB :
			hlen += MIN(sizeof(struct icmp) + 8, fin->fin_dlen);
			break;
		default :
			hlen += MIN(sizeof(struct icmp), fin->fin_dlen);
			break;
		}
	}

	mlen = (flags & FR_LOGBODY) ? (MIN(m->m_len, 128) & 0xfa) : 0;
	len = hlen + sizeof(iplci) + mlen;
	if (iplused + len > LOGSIZE)
		return 0;
	iplused += len;

# ifdef	sun
	uniqtime(&iplci);
# endif
# if BSD >= 199306 || defined(__FreeBSD__)
	microtime((struct timeval *)&iplci);
# endif
	iplci.flags = flags;
	iplci.hlen = (u_char)hlen;
	iplci.plen = (flags & FR_LOGBODY) ? (u_char)mlen : 0 ;
	iplci.rule = fin->fin_rule;
#if !defined (__OpenBSD__) && !defined (__NetBSD__)
	iplci.unit = (u_char)ifp->if_unit;
	iplci.ifname[0] = ifp->if_name[0];
	iplci.ifname[1] = ifp->if_name[1];
	iplci.ifname[2] = ifp->if_name[2];
	iplci.ifname[3] = ifp->if_name[3];
#else /* OpenBSD or NetBSD */
	bcopy (ifp->if_xname, iplci.ifname, IFNAMSIZ);
#endif /* OpenBSD or NetBSD */

	if (iplh == iplbuf + LOGSIZE)
		iplh = iplbuf;
	tail = (iplh >= iplt) ? (iplbuf + LOGSIZE - iplh) : (iplt - iplh);

	len = MIN(tail, sizeof(iplci));

	/*
	 * check in both cases where we add stuff to the buffer to see if we
	 * are going to wrap around at the end.
	 */
	bcopy((char *)&iplci, iplh, len);
	iplh += len;
	if (len < sizeof(iplci)) {
		bcopy((char *)&iplci + len, iplbuf, sizeof(iplci) - len);
		iplh = iplbuf + sizeof(iplci) - len;
		tail = iplt - iplh;
	} else
		tail -= len;

	len = MIN(tail, hlen);
	bcopy((char *)ip, iplh, len);
	iplh += len;
	if (len < hlen) {
		iplh = iplbuf;
		bcopy((char *)ip + len, iplh, hlen - len);
		iplh += hlen - len;
		tail = iplt - iplh;
	} else
		tail -= len;

	if (mlen) {
		len = MIN(tail, mlen);
#if BSD < 199103
		bcopy((char *)m->m_un.mun_dat, iplh, len);
#else
		bcopy((char *)m->M_dat.M_databuf, iplh, len);
#endif
		iplh += len;
		if (len < mlen) {
			iplh = iplbuf;
#if BSD < 199103
			bcopy((char *)m->m_un.mun_dat + len, iplh,
				mlen - len);
#else
			bcopy((char *)m->M_dat.M_databuf + len, iplh,
				mlen - len);
#endif
			iplh += mlen - len;
		}
	}
	wakeup(iplbuf);
	return 1;
}
#endif /* IPFILTER_LOG */

/*
 * send_reset - this could conceivably be a call to tcp_respond(), but that
 * requires a large amount of setting up and isn't any more efficient.
 */
int send_reset(ti)
struct tcpiphdr *ti;
{
	struct tcpiphdr *tp;
	struct ip *ip;
	struct tcphdr *tcp;
	struct mbuf *m;
	int tlen = 0;

	if (ti->ti_flags & TH_RST)
		return -1;		/* feedback loop */
#if	BSD < 199306
	m = m_get(M_DONTWAIT, MT_HEADER);
#else
	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	m->m_data += max_linkhdr;
#endif
	if (m == NULL)
		return -1;

	if (ti->ti_flags & TH_SYN)
		tlen = 1;
	m->m_len = sizeof (struct tcpiphdr);
#if	BSD >= 199306
	m->m_pkthdr.len = sizeof (struct tcpiphdr);
	m->m_pkthdr.rcvif = (struct ifnet *)0;
#endif
	bzero(mtod(m, char *), sizeof(struct tcpiphdr));
	ip = mtod(m, struct ip *);
	tp = mtod(m, struct tcpiphdr *);
	tcp = (struct tcphdr *)((char *)ip + sizeof(struct ip));

	ip->ip_src.s_addr = ti->ti_dst.s_addr;
	ip->ip_dst.s_addr = ti->ti_src.s_addr;
	tcp->th_dport = ti->ti_sport;
	tcp->th_sport = ti->ti_dport;
	tcp->th_ack = htonl(ntohl(ti->ti_seq) + tlen);
	tcp->th_off = sizeof(struct tcphdr) >> 2;
	tcp->th_flags = TH_RST|TH_ACK;
	tp->ti_pr = ((struct ip *)ti)->ip_p;
	tp->ti_len = htons(sizeof(struct tcphdr));
	tcp->th_sum = in_cksum(m, sizeof(struct tcpiphdr));

	ip->ip_hl = sizeof(struct ip) >> 2;
	ip->ip_v = IPVERSION;
	ip->ip_tos = ((struct ip *)ti)->ip_tos;
	ip->ip_id = ((struct ip *)ti)->ip_id;
	ip->ip_off = ((struct ip *)ti)->ip_off;
	ip->ip_p = ((struct ip *)ti)->ip_p;
	ip->ip_len = sizeof (struct tcpiphdr);
#if BSD < 199306
	ip->ip_ttl = tcp_ttl;
#else
	ip->ip_ttl = ip_defttl;
#endif

	/*
	 * extra 0 in case of multicast
	 */
	(void) ip_output(m, (struct mbuf *)0, 0, IP_FORWARDING, 0);
	return 0;
}


#ifndef	IPFILTER_LKM
iplinit()
{
	(void) iplattach();
	ip_init();
}
#endif
