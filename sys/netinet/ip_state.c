/*
 * (C)opyright 1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#ifndef	lint
static	char	sccsid[] = "@(#)ip_state.c	1.6 3/24/96 (C) 1993-1995 Darren Reed";
#endif

#if !defined(_KERNEL) && !defined(KERNEL)
# include <stdlib.h>
# include <string.h>
#endif
#ifndef	linux
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#if !defined(__SVR4) && !defined(__svr4__)
# include <sys/dir.h>
# include <sys/mbuf.h>
#else
# include <sys/byteorder.h>
# include <sys/dditypes.h>
# include <sys/stream.h>
# include <sys/kmem.h>
#endif

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
#include "ip_state.h"
#ifndef	MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif

#define	TCP_CLOSE	(TH_FIN|TH_RST)

ipstate_t *ips_table[IPSTATE_SIZE];
int	ips_num = 0;
ips_stat_t ips_stats;
#if	SOLARIS
extern	kmutex_t	ipf_state;
# if	!defined(_KERNEL)
#define	bcopy(a,b,c)	memmove(b,a,c)
# endif
#endif


ips_stat_t *fr_statetstats()
{
	ips_stats.iss_active = ips_num;
	ips_stats.iss_table = ips_table;
	return &ips_stats;
}


#define	PAIRS(s1,d1,s2,d2)	((((s1) == (s2)) && ((d1) == (d2))) ||\
				 (((s1) == (d2)) && ((d1) == (s2))))
#define	IPPAIR(s1,d1,s2,d2)	PAIRS((s1).s_addr, (d1).s_addr, \
				      (s2).s_addr, (d2).s_addr)

/*
 * Create a new ipstate structure and hang it off the hash table.
 */
int fr_addstate(ip, hlen, pass)
ip_t *ip;
int hlen;
u_int pass;
{
	ipstate_t ips;
	register ipstate_t *is = &ips;
	register u_int hv;

	if (ips_num == IPSTATE_MAX) {
		ips_stats.iss_max++;
		return -1;
	}
	/*
	 * Copy and calculate...
	 */
	hv = (is->is_p = ip->ip_p);
	hv += (is->is_src.s_addr = ip->ip_src.s_addr);
	hv += (is->is_dst.s_addr = ip->ip_dst.s_addr);

	switch (ip->ip_p)
	{
	case IPPROTO_ICMP :
	    {
		struct icmp *ic = (struct icmp *)((char *)ip + hlen);

		switch (ic->icmp_type)
		{
		case ICMP_ECHO :
			is->is_icmp.ics_type = 0;
			hv += (is->is_icmp.ics_id = ic->icmp_id);
			hv += (is->is_icmp.ics_seq = ic->icmp_seq);
			break;
		case ICMP_TSTAMP :
		case ICMP_IREQ :
		case ICMP_MASKREQ :
			is->is_icmp.ics_type = ic->icmp_type + 1;
			break;
		default :
			return -1;
		}
		ips_stats.iss_icmp++;
		is->is_age = 120;
		break;
	    }
	case IPPROTO_TCP :
	    {
		register tcphdr_t *tcp = (tcphdr_t *)((char *)ip + hlen);

		/*
		 * The endian of the ports doesn't matter, but the ack and
		 * sequence numbers do as we do mathematics on them later.
		 */
		hv += (is->is_dport = tcp->th_dport);
		hv += (is->is_sport = tcp->th_sport);
		is->is_seq = ntohl(tcp->th_seq);
		is->is_ack = ntohl(tcp->th_ack);
		is->is_win = ntohs(tcp->th_win);
		ips_stats.iss_tcp++;
		/*
		 * If we're creating state for a starting connectoin, start the
		 * timer on it as we'll never see an error if it fails to
		 * connect.
		 */
		if ((tcp->th_flags & (TH_SYN|TH_ACK)) == TH_SYN)
			is->is_age = 120;
		else
			is->is_age = 0;
		break;
	    }
	case IPPROTO_UDP :
	    {
		register tcphdr_t *tcp = (tcphdr_t *)((char *)ip + hlen);

		hv += (is->is_dport = tcp->th_dport);
		hv += (is->is_sport = tcp->th_sport);
		ips_stats.iss_udp++;
		is->is_age = 120;
		break;
	    }
	default :
		return -1;
	}

	if (!(is = (ipstate_t *)KMALLOC(sizeof(*is)))) {
		ips_stats.iss_nomem++;
		return -1;
	}
	bcopy((char *)&ips, (char *)is, sizeof(*is));
	hv %= IPSTATE_SIZE;
	MUTEX_ENTER(&ipf_state);
	is->is_next = ips_table[hv];
	ips_table[hv] = is;
	is->is_pass = pass & ~(FR_LOGFIRST|FR_LOG);
	ips_num++;
	MUTEX_EXIT(&ipf_state);
	return 0;
}


/*
 * Check if a packet has a registered state.
 */
int fr_checkstate(ip, fin)
ip_t *ip;
fr_info_t *fin;
{
	register struct in_addr dst, src;
	register ipstate_t *is, **isp;
	register u_char pr;
	struct icmp *ic;
	tcphdr_t *tcp;
	u_int hv, hlen;

	if ((ip->ip_off & 0x1fff) && !(fin->fin_fi.fi_fl & FI_SHORT))
		return 0;

	hlen = fin->fin_hlen;
	tcp = (tcphdr_t *)((char *)ip + hlen);
	ic = (struct icmp *)tcp;
	hv = (pr = ip->ip_p);
	hv += (src.s_addr = ip->ip_src.s_addr);
	hv += (dst.s_addr = ip->ip_dst.s_addr);

	/*
	 * Search the hash table for matching packet header info.
	 */
	switch (ip->ip_p)
	{
	case IPPROTO_ICMP :
		hv += ic->icmp_id;
		hv += ic->icmp_seq;
		hv %= IPSTATE_SIZE;
		MUTEX_ENTER(&ipf_state);
		for (isp = &ips_table[hv]; (is = *isp); isp = &is->is_next)
			if ((is->is_p == pr) &&
			    (ic->icmp_id == is->is_icmp.ics_id) &&
			    (ic->icmp_seq == is->is_icmp.ics_seq) &&
			    IPPAIR(src, dst, is->is_src, is->is_dst)) {
				/*
				 * If we have type 0 stored, allow any icmp
				 * replies through.
				 */
				if (is->is_icmp.ics_type &&
				    is->is_icmp.ics_type != ic->icmp_type)
					continue;
				is->is_age = 120;
				ips_stats.iss_hits++;
				MUTEX_EXIT(&ipf_state);
				return is->is_pass;
			}
		MUTEX_EXIT(&ipf_state);
		break;
	case IPPROTO_TCP :
	    {
		register u_short dport = tcp->th_dport, sport = tcp->th_sport;
		register u_short win = ntohs(tcp->th_win);
		tcp_seq seq, ack;

		hv += dport;
		hv += sport;
		hv %= IPSTATE_SIZE;
		MUTEX_ENTER(&ipf_state);
		for (isp = &ips_table[hv]; (is = *isp); isp = &is->is_next) {
			register int dl, seqskew, ackskew;

			if ((is->is_p == pr) &&
			    PAIRS(sport, dport, is->is_sport, is->is_dport) &&
			    IPPAIR(src, dst, is->is_src, is->is_dst)) {
				dl = ip->ip_len - hlen - sizeof(tcphdr_t);
				/*
				 * Find difference between last checked packet
				 * and this packet.
				 */
				seq = ntohl(tcp->th_seq);
				ack = ntohl(tcp->th_ack);
				if (sport == is->is_sport) {
					seqskew = seq - is->is_seq;
					ackskew = ack - is->is_ack;
				} else {
					seqskew = ack - is->is_seq;
					if (!is->is_ack) {
						/*
						 * Must be a SYN-ACK in reply
						 * to a SYN.  Set age timeout
						 * to 0 to stop deletion.
						 */
						is->is_ack = seq;
						is->is_age = 0;
					}
					ackskew = seq - is->is_ack;
				}

				/*
				 * Make skew values absolute
				 */
				if (seqskew < 0)
					seqskew = -seqskew;
				if (ackskew < 0)
					ackskew = -ackskew;
				/*
				 * If the difference in sequence and ack
				 * numbers is within the window size of the
				 * connection, store these values and match
				 * the packet.
				 */
				if ((seqskew <= win) && (ackskew <= win)) {
					is->is_win = win;
					if (sport == is->is_sport) {
						is->is_seq = seq;
						is->is_ack = ack;
					} else {
						is->is_seq = ack;
						is->is_ack = seq;
					}
					ips_stats.iss_hits++;
					/*
					 * Nearing end of connection, start
					 * timeout.
					 */
#ifdef	_KERNEL
					if ((tcp->th_flags & TCP_CLOSE) &&
					    !is->is_age)
						is->is_age = 120;
					MUTEX_EXIT(&ipf_state);
					return is->is_pass;
#else
					if (tcp->th_flags & TCP_CLOSE) {
						int pass = is->is_pass;

						*isp = is->is_next;
						isp = &ips_table[hv];
						KFREE(is);
						return pass;
					}
					return is->is_pass;
#endif
				}
			}
		}
		MUTEX_EXIT(&ipf_state);
		break;
	    }
	case IPPROTO_UDP :
	    {
		register u_short dport = tcp->th_dport, sport = tcp->th_sport;

		hv += dport;
		hv += sport;
		hv %= IPSTATE_SIZE;
		/*
		 * Nothing else to match on but ports. and IP#'s
		 */
		MUTEX_ENTER(&ipf_state);
		for (is = ips_table[hv]; is; is = is->is_next)
			if ((is->is_p == pr) &&
			    PAIRS(sport, dport, is->is_sport, is->is_dport) &&
			    IPPAIR(src, dst, is->is_src, is->is_dst)) {
				ips_stats.iss_hits++;
				is->is_age = 120;
				MUTEX_EXIT(&ipf_state);
				return is->is_pass;
			}
		MUTEX_EXIT(&ipf_state);
		break;
	    }
	default :
		break;
	}
	ips_stats.iss_miss++;
	return 0;
}


/*
 * Free memory in use by all state info. kept.
 */
void fr_stateunload()
{
	register int i;
	register ipstate_t *is, **isp;

	MUTEX_ENTER(&ipf_state);
	for (i = 0; i < IPSTATE_SIZE; i++)
		for (isp = &ips_table[i]; (is = *isp); ) {
			*isp = is->is_next;
			KFREE(is);
		}
	MUTEX_EXIT(&ipf_state);
}


/*
 * Slowly expire held state for thingslike UDP and ICMP.  Timeouts are set
 * in expectation of this being called twice per second.
 */
void fr_timeoutstate()
{
	register int i;
	register ipstate_t *is, **isp;

	MUTEX_ENTER(&ipf_state);
	for (i = 0; i < IPSTATE_SIZE; i++)
		for (isp = &ips_table[i]; (is = *isp); )
			if (is->is_age && !--is->is_age) {
				*isp = is->is_next;
				if (is->is_p == IPPROTO_TCP)
					ips_stats.iss_fin++;
				else
					ips_stats.iss_expire++;
				KFREE(is);
				ips_num--;
			} else
				isp = &is->is_next;
	MUTEX_EXIT(&ipf_state);
}
