/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#ifndef	lint
static	char	sccsid[] = "@(#)fil.c	1.18 10/24/95 (C) 1993-1995 Darren Reed";
#endif

#ifndef	linux
# include <sys/errno.h>
# include <sys/types.h>
# include <sys/param.h>
# include <sys/file.h>
# include <sys/ioctl.h>
# if defined(_KERNEL) || defined(KERNEL)
#  include <sys/systm.h>
# endif
# include <sys/uio.h>
# if !defined(__SVR4) && !defined(__svr4__)
#  include <sys/dir.h>
#  include <sys/mbuf.h>
# else
#  include <sys/byteorder.h>
#  include <sys/dditypes.h>
#  include <sys/stream.h>
# endif
# include <sys/protosw.h>
# include <sys/socket.h>
# include <net/if.h>
# ifdef sun
#  include <net/af.h>
# endif
# include <net/route.h>
# include <netinet/in.h>
# include <netinet/in_systm.h>
# include <netinet/ip.h>
# include <netinet/ip_var.h>
# include <netinet/tcp.h>
# include <netinet/udp.h>
# include <netinet/tcpip.h>
# include <netinet/ip_icmp.h>
#endif
#include <netinet/ip_fil.h>
#ifndef	MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif

#ifndef	_KERNEL
#include "ipf.h"
extern	int	opts;
extern	void	debug(), verbose();

#define	FR_IFVERBOSE(ex,second,verb_pr)	if (ex) { verbose verb_pr; second; }
#define	FR_IFDEBUG(ex,second,verb_pr)	if (ex) { debug verb_pr; second; }
#define	FR_VERBOSE(verb_pr)	verbose verb_pr
#define	FR_DEBUG(verb_pr)	debug verb_pr
#else
#define	FR_IFVERBOSE(ex,second,verb_pr)	;
#define	FR_IFDEBUG(ex,second,verb_pr)	;
#define	FR_VERBOSE(verb_pr)
#define	FR_DEBUG(verb_pr)

extern	int	ipl_unreach, ipllog();
#endif

struct	filterstats frstats[2] = {{0,0,0,0,0},{0,0,0,0,0}};
struct	frentry	*filterin[2] = { NULL, NULL },
		*filterout[2] = { NULL, NULL };
int	fr_flags = 0, fr_active = 0;
int	fr_check();

 
/*
 * bit values for identifying presence of individual IP options
 */
struct	optlist	ipopts[20] = {
	{ IPOPT_NOP,	0x000001 },
	{ IPOPT_RR,	0x000002 },
	{ IPOPT_ZSU,	0x000004 },
	{ IPOPT_MTUP,	0x000008 },
	{ IPOPT_MTUR,	0x000010 },
	{ IPOPT_ENCODE,	0x000020 },
	{ IPOPT_TS,	0x000040 },
	{ IPOPT_TR,	0x000080 },
	{ IPOPT_SECURITY, 0x000100 },
	{ IPOPT_LSRR,	0x000200 },
	{ IPOPT_E_SEC,	0x000400 },
	{ IPOPT_CIPSO,	0x000800 },
	{ IPOPT_SATID,	0x001000 },
	{ IPOPT_SSRR,	0x002000 },
	{ IPOPT_ADDEXT,	0x004000 },
	{ IPOPT_VISA,	0x008000 },
	{ IPOPT_IMITD,	0x010000 },
	{ IPOPT_EIP,	0x020000 },
	{ IPOPT_FINN,	0x040000 },
	{ 0,		0x000000 }
};

/*
 * bit values for identifying presence of individual IP security options
 */
struct	optlist	secopt[8] = {
	{ IPSO_CLASS_RES4,	0x01 },
	{ IPSO_CLASS_TOPS,	0x02 },
	{ IPSO_CLASS_SECR,	0x04 },
	{ IPSO_CLASS_RES3,	0x08 },
	{ IPSO_CLASS_CONF,	0x10 },
	{ IPSO_CLASS_UNCL,	0x20 },
	{ IPSO_CLASS_RES2,	0x40 },
	{ IPSO_CLASS_RES1,	0x80 }
};


/*
 * compact the IP header into a structure which contains just the info.
 * which is useful for comparing IP headers with.
 */
struct fr_ip *fr_makefrip(hlen, ip)
int hlen;
ip_t *ip;
{
	static struct fr_ip fi;
	struct optlist *op;
	u_short optmsk = 0, secmsk = 0, auth = 0;
	int i, mv, ol, off;
	u_char *s, opt;

	fi.fi_fl = 0;
	fi.fi_v = ip->ip_v;
	fi.fi_tos = ip->ip_tos;
	(*(((u_short *)&fi) + 1)) = (*(((u_short *)ip) + 4));
	(*(((u_long *)&fi) + 1)) = (*(((u_long *)ip) + 3));
	(*(((u_long *)&fi) + 2)) = (*(((u_long *)ip) + 4));

	if (hlen > sizeof(struct ip))
		fi.fi_fl |= FI_OPTIONS;
	off = (ip->ip_off & 0x1fff) << 3;
	if (ip->ip_off & 0x3fff)
		fi.fi_fl |= FI_FRAG;
	switch (ip->ip_p)
	{
	case IPPROTO_ICMP :
		if ((!IPMINLEN(ip, icmp) && !off) ||
		    (off && off < sizeof(struct icmp)))
			fi.fi_fl |= FI_SHORT;
		break;
	case IPPROTO_TCP :
		fi.fi_fl |= FI_TCPUDP;
		if ((!IPMINLEN(ip, tcphdr) && !off) ||
		    (off && off < sizeof(struct tcphdr)))
			fi.fi_fl |= FI_SHORT;
		break;
	case IPPROTO_UDP :
		fi.fi_fl |= FI_TCPUDP;
		if ((!IPMINLEN(ip, udphdr) && !off) ||
		    (off && off < sizeof(struct udphdr)))
			fi.fi_fl |= FI_SHORT;
		break;
	default :
		break;
	}

	for (s = (u_char *)(ip + 1), hlen -= sizeof(*ip); hlen; ) {
		if (!(opt = *s))
			break;
		ol = (opt == IPOPT_NOP) ? 1 : (int)*(s+1);
		if (opt > 1 && (ol < 0 || ol > hlen))
			break;
		for (i = 9, mv = 4; mv >= 0; ) {
			op = ipopts + i;
			if (opt == (u_char)op->ol_val) {
				optmsk |= op->ol_bit;
				if (opt == IPOPT_SECURITY) {
					struct optlist *sp;
					u_char	sec;
					int j, m;

					sec = *(s + 3);	/* classification */
					for (j = 3, m = 2; m >= 0; ) {
						sp = secopt + j;
						if (sec == sp->ol_val) {
							secmsk |= sp->ol_bit;
							auth = *(s + 3);
							auth *= 256;
							auth += *(s + 4);
							break;
						}
						if (sec < sp->ol_val)
							j -= m--;
						else
							j += m--;
					}
				}
				break;
			}
			if (opt < op->ol_val)
				i -= mv--;
			else
				i += mv--;
		}
		hlen -= ol;
		s += ol;
	}
	if (auth && !(auth & 0x0100))
		auth &= 0xff00;
	fi.fi_optmsk = optmsk;
	fi.fi_secmsk = secmsk;
	fi.fi_auth = auth;
	return &fi;
}


/*
 * check an IP packet for TCP/UDP characteristics such as ports and flags.
 */
int fr_tcpudpchk(ip, tcp, fr)
ip_t *ip;
tcphdr_t *tcp;
struct frentry *fr;
{
	register u_short po, tup;
	register char i;
	int err = 1;

	/*
	 * Both ports should *always* be in the first fragment.
	 * So far, I cannot find any cases where they can not be.
	 *
	 * compare destination ports
	 */
	if ((i = (int)fr->fr_dcmp)) {
		po = ntohs(fr->fr_dport);
		tup = ntohs(tcp->th_dport);
		/*
		 * Do opposite test to that required and
		 * continue if that succeeds.
		 */
		if (!--i && tup != po) /* EQUAL */
			err = 0;
		else if (!--i && tup == po) /* NOTEQUAL */
			err = 0;
		else if (!--i && tup >= po) /* LESSTHAN */
			err = 0;
		else if (!--i && tup <= po) /* GREATERTHAN */
			err = 0;
		else if (!--i && tup > po) /* LT or EQ */
			err = 0;
		else if (!--i && tup < po) /* GT or EQ */
			err = 0;
		else if (!--i &&	   /* Out of range */
			 (tup >= po && tup <= ntohs(fr->fr_dtop)))
			err = 0;
		else if (!--i &&	   /* In range */
			 (tup <= po || tup >= ntohs(fr->fr_dtop)))
			err = 0;
	}
	/*
	 * compare source ports
	 */
	if (err && (i = (int)fr->fr_scmp)) {
		po = ntohs(fr->fr_sport);
		tup = ntohs(tcp->th_sport);
		if (!--i && tup != po)
			err = 0;
		else if (!--i && tup == po)
			err = 0;
		else if (!--i && tup >= po)
			err = 0;
		else if (!--i && tup <= po)
			err = 0;
		else if (!--i && tup > po)
			err = 0;
		else if (!--i && tup < po)
			err = 0;
		else if (!--i &&	   /* Out of range */
			 (tup >= po && tup <= ntohs(fr->fr_stop)))
			err = 0;
		else if (!--i &&	   /* In range */
			 (tup <= po || tup >= ntohs(fr->fr_stop)))
			err = 0;
	}

	/*
	 * If we don't have all the TCP/UDP header, then how can we
	 * expect to do any sort of match on it ?  If we were looking for
	 * TCP flags, then NO match.  If not, then match (which should
	 * satisfy the "short" class too).
	 */
	if (err)
		if (ip->ip_p == IPPROTO_TCP) {
			if (!IPMINLEN(ip, tcphdr))
				return !(fr->fr_tcpf);
			/*
			 * Match the flags ?  If not, abort this match.
			 */
			if (fr->fr_tcpf &&
			    fr->fr_tcpf != (tcp->th_flags & fr->fr_tcpfm)) {
				FR_DEBUG(("f. %#x & %#x != %#x\n",
					 tcp->th_flags, fr->fr_tcpfm,
					 fr->fr_tcpf));
				err = 0;
			}
		}
		else if (!IPMINLEN(ip, udphdr))	/* must be UDP */
			return 1;
	return err;
}

/*
 * Check the input/output list of rules for a match and result.
 * Could be per interface, but this gets real nasty when you don't have
 * kernel sauce.
 */
int fr_scanlist(pass, ip, hlen, ifp, out, rule)
int pass;
ip_t *ip;
int hlen, out;
struct ifnet *ifp;
u_short *rule;
{
	register struct frentry *fr;
	register struct fr_ip *fi;
	tcphdr_t *tcp;
	int	rulen;

	*rule = 1;
	tcp = (tcphdr_t *)((char *)ip + hlen);
	fr = (out) ? filterout[fr_active] : filterin[fr_active];
	fi = fr_makefrip(hlen, ip);

	for (rulen = 0; fr; fr = fr->fr_next, rulen++) {
		/*
		 * In all checks below, a null (zero) value in the
		 * filter struture is taken to mean a wildcard.
		 *
		 * check that we are working for the right interface
		 */
#ifdef	_KERNEL
		if (fr->fr_ifa && fr->fr_ifa != ifp)
			continue;
#else
		if (opts & (OPT_VERBOSE|OPT_DEBUG))
			printf("\n");
		FR_VERBOSE(("%c", (pass & FR_PASS) ? 'p' : 'b'));
		if (ifp && *fr->fr_ifname && strcasecmp(ifp->if_name,
							fr->fr_ifname))
			continue;
		FR_VERBOSE((":i"));
#endif
		{
			register u_long	*ld, *lm, *lip;
			register int i;

			lip = (u_long *)fi;
			lm = (u_long *)&fr->fr_mip;
			ld = (u_long *)&fr->fr_ip;
			i = ((lip[0] & lm[0]) != ld[0]);
			FR_IFDEBUG(i,continue,("0. %#08x & %#08x != %#08x\n",
				   lip[0], lm[0], ld[0]));
			i |= ((lip[1] & lm[1]) != ld[1]);
			FR_IFDEBUG(i,continue,("1. %#08x & %#08x != %#08x\n",
				   lip[1], lm[1], ld[1]));
			i |= ((lip[2] & lm[2]) != ld[2]);
			FR_IFDEBUG(i,continue,("2. %#08x & %#08x != %#08x\n",
				   lip[2], lm[2], ld[2]));
			i |= ((lip[3] & lm[3]) != ld[3]);
			FR_IFDEBUG(i,continue,("3. %#08x & %#08x != %#08x\n",
				   lip[3], lm[3], ld[3]));
			i |= ((lip[4] & lm[4]) != ld[4]);
			FR_IFDEBUG(i,continue,("4. %#08x & %#08x != %#08x\n",
				   lip[4], lm[4], ld[4]));
			if (i)
				continue;
		}

		/*
		 * If a fragment, then only the first has what we're looking
		 * for here...
		 */
		if (!(ip->ip_off & 0x1fff)) {
			if ((fi->fi_fl & FI_TCPUDP) &&
			    !fr_tcpudpchk(ip, tcp, fr))
				continue;
			else if (ip->ip_p == IPPROTO_ICMP &&
				   (*(u_short *)((char *)ip + hlen) &
				     fr->fr_icmpm) != fr->fr_icmp) {
				FR_DEBUG(("i. %#x & %#x != %#x\n",
					 *(u_short *)((char *)ip + hlen),
					 fr->fr_icmpm, fr->fr_icmp));
				continue;
			}
		} else if (fr->fr_dcmp || fr->fr_scmp || fr->fr_icmpm ||
			   fr->fr_tcpfm)
			continue;
		FR_VERBOSE(("*"));
		/*
		 * Just log this packet...
		 */
		if (fr->fr_flags & FR_LOG) {
#ifdef	IPFILTER_LOG
			if (!ipllog(hlen, fr->fr_flags, ip, ifp, *rule))
				frstats[out].fr_skip++;
			frstats[out].fr_pkl++;
#endif /* IPFILTER_LOG */
		} else
			pass = fr->fr_flags;
		FR_DEBUG(("pass %#x\n", pass));
		fr->fr_hits++;
		*rule = rulen;
		if (pass & FR_QUICK)
			break;
	}
	return pass;
}


/*
 * frcheck - filter check
 * check using source and destination addresses/pors in a packet whether
 * or not to pass it on or not.
 */
int fr_check(ip, hlen, ifp, out
#if SOLARIS && defined(_KERNEL)
, qif, q)
qif_t *qif;
queue_t *q;
#else
)
#endif
ip_t *ip;
int hlen;
struct ifnet *ifp;
int out;
{
	int pass = FR_NOMATCH;
	int sl;
	u_short rule;

	SPLNET(sl);

	pass = fr_scanlist(pass, ip, hlen, ifp, out, &rule);
	if (pass == FR_NOMATCH) {
		frstats[out].fr_nom++;
#ifdef	NOMATCH
		pass |= NOMATCH;
#endif
	}

#ifdef	IPFILTER_LOG
	if ((pass & FR_LOGP) ||
	    ((pass & FR_PASS) && (fr_flags & FF_LOGPASS))) {
		if (!(pass & FR_LOGP))
			pass |= FF_LOGPASS << 8;
		if (!ipllog(hlen, pass, ip, ifp, rule))
			frstats[out].fr_skip++;
		frstats[out].fr_ppkl++;
	} else if ((pass & FR_LOGB) ||
		   ((pass & FR_BLOCK) && (fr_flags & FF_LOGBLOCK))) {
		if (!(pass & FR_LOGB))
			pass |= FF_LOGBLOCK << 8;
		if (!ipllog(hlen, pass, ip, ifp, rule))
			frstats[out].fr_skip++;
		frstats[out].fr_bpkl++;
	}
#endif /* IPFILTER_LOG */
	SPLX(sl);
	if (pass & FR_PASS)
		frstats[out].fr_pass++;
	else if (pass & FR_BLOCK) {
		frstats[out].fr_block++;
		/*
		 * Should we return an ICMP packet to indicate error
		 * status passing through the packet filter ?
		 * XXX - copy mbuf as icmp_error() calls mfree() - fix this
		 * later, but preserve backward compatibility for now.
		 */
#ifdef	_KERNEL
		if (pass & FR_RETICMP) {
# if SOLARIS
			icmp_error(q, ip, ICMP_UNREACH, ipl_unreach, qif,
				   ip->ip_src);
# else
			struct mbuf *copy;

			copy = m_copy(dtom(ip), 0, imin((int)ip->ip_len, 64));
#  if BSD < 199103
			icmp_error(mtod(copy, struct ip *),
				   ICMP_UNREACH, ipl_unreach, ifp, ip->ip_src);
#  else
			icmp_error(copy, ICMP_UNREACH, ipl_unreach,
				   ip->ip_src.s_addr, ifp);
#  endif
# endif
			frstats[0].fr_ret++;
		} else if (pass & FR_RETRST && IPMINLEN(ip, tcphdr)) {
# if SOLARIS
			if (send_reset(ip, qif, q) == 0)
# else
			if (send_reset(ip) == 0)
# endif
				frstats[1].fr_ret++;
		}
#else
		if (pass & FR_RETICMP) {
			verbose("- ICMP unreachable sent\n");
			frstats[0].fr_ret++;
		} else if (pass & FR_RETRST && IPMINLEN(ip, tcphdr)) {
			verbose("- TCP RST sent\n");
			frstats[1].fr_ret++;
		}
#endif
	}
#ifdef	_KERNEL
	return (pass & FR_PASS) ? 0 : -1;
#else
	if (pass & FR_NOMATCH)
		return 1;
	if (pass & FR_PASS)
		return 0;
	return -1;
#endif
}


#ifndef	_KERNEL
int ipllog()
{
	verbose("l");
	return 1;
}
#endif
