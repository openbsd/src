/*	$OpenBSD: encap.c,v 1.23 1998/05/24 14:13:57 provos Exp $	*/

/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and 
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece, 
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Copyright (C) 1995, 1996, 1997, 1998 by John Ioannidis, Angelos D. Keromytis
 * and Niels Provos.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software. 
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/ioctl.h>
#include <vm/vm.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>
#include <machine/stdarg.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#endif 

#include <net/encap.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_ip4.h>

#include <sys/syslog.h>

void encap_init(void);
void encap_sendnotify(int, struct tdb *, void *);
int encap_notify_sa(u_int32_t, struct in_addr, struct in_addr, 
    u_int16_t, u_int16_t, u_int16_t, u_int16_t);
int encap_enable_spi(u_int32_t, struct in_addr, struct in_addr, struct in_addr,
    struct in_addr, struct in_addr, u_int16_t, u_int16_t, 
    u_int16_t, u_int16_t, u_int16_t);
int encap_output __P((struct mbuf *, ...));
int encap_usrreq(struct socket *, int, struct mbuf *, struct mbuf *, 
		 struct mbuf *);
int encap_sysctl(int *, u_int, void *, size_t *, void *, size_t);

extern int tdb_init(struct tdb *, struct mbuf *);

extern struct domain encapdomain;

extern struct inpcbtable tcbtable; /* Notify - XXX */
extern struct inpcbtable udbtable; /* Notify - XXX */
extern struct inpcbtable rawcbtable; /* Notify - XXX */

struct sockaddr encap_dst = { 2, PF_ENCAP, };
struct sockaddr encap_src = { 2, PF_ENCAP, };
struct sockproto encap_proto = { PF_ENCAP, };

struct protosw encapsw[] = { 
    { SOCK_RAW,	&encapdomain,	0,		PR_ATOMIC|PR_ADDR,
      raw_input,	encap_output,	raw_ctlinput,	0,
      encap_usrreq,
      encap_init,	0,		0,		0,
      encap_sysctl
    },
};

struct domain encapdomain =
{ AF_ENCAP, "encapsulation", 0, 0, 0, 
  encapsw, &encapsw[sizeof(encapsw) / sizeof(encapsw[0])], 0,
  rn_inithead, 16, sizeof(struct sockaddr_encap)};

/*
 * Sysctl for encap variables
 */
int
encap_sysctl(int *name, u_int namelen, void *oldp, size_t *oldplenp, 
	     void *newp, size_t newlen)
{
    /* All sysctl names at this level are terminal */
    if (namelen != 1)
      return ENOTDIR;

    switch (name[0]) 
    {
        case ENCAPCTL_ENCDEBUG:
	    return (sysctl_int(oldp, oldplenp, newp, newlen, &encdebug));

	default:
	    return ENOPROTOOPT;
    }
    /* Not reached */
}

void
encap_init()
{
    struct xformsw *xsp;
    
    for (xsp = xformsw; xsp < xformswNXFORMSW; xsp++)
    {
	/*log(LOG_INFO, "encap_init(): attaching <%s>\n", xsp->xf_name);*/
	(*(xsp->xf_attach))();
    }
}

/*ARGSUSED*/
int
encap_usrreq(register struct socket *so, int req, struct mbuf *m, 
	     struct mbuf *nam, struct mbuf *control)
{
    register struct rawcb *rp = sotorawcb(so);
    register int error = 0;
    int s;
    
    if (req == PRU_ATTACH)
    {
	MALLOC(rp, struct rawcb *, sizeof(*rp), M_PCB, M_WAITOK);
        if (rp == (struct rawcb *) NULL)
	  return ENOBUFS;

	if ((so->so_pcb = (caddr_t) rp))
	  bzero(so->so_pcb, sizeof(*rp));
    }

    s = splnet();
    error = raw_usrreq(so, req, m, nam, control);
    rp = sotorawcb(so);
    if ((req == PRU_ATTACH) && rp)
    {
	/* int af = rp->rcb_proto.sp_protocol; */
	
	if (error)
	{
	    free((caddr_t) rp, M_PCB);
	    splx(s);
	    return error;
	}
	rp->rcb_faddr = &encap_src;
	soisconnected(so);
	so->so_options |= SO_USELOOPBACK;
    }
    splx(s);
    return error;
}

int
encap_notify_sa(u_int32_t spi, struct in_addr dst, struct in_addr src,
		u_int16_t sport, u_int16_t dport, u_int16_t protocol, 
		u_int16_t sproto)
{
	struct inpcbtable *table = NULL;
	struct inpcb *inp = NULL;
	struct in_addr altm, zeroin_addr;
	struct tdb *tdbp;
	struct flow *flow;
	int error = 0;
	u_int8_t secrequire;

	altm.s_addr = INADDR_BROADCAST;
    
	switch (protocol) {
	case IPPROTO_TCP:
		table = &tcbtable;
		break;
	case IPPROTO_UDP:
		table = &udbtable;
		break;
	default:
		break;
	}

	if (table != NULL) {
		/* Protocols with own inpcb tables */
		bzero((caddr_t)&zeroin_addr, sizeof(zeroin_addr));
		inp = in_pcblookup(table, dst, dport, zeroin_addr, sport, 
				   INPLOOKUP_WILDCARD);
	} else {
		/* RAW protocol - taken from raw_ip.c */
		/* XXX - we can have more than one inp sleeping here */
		for (inp = rawcbtable.inpt_queue.cqh_first;
		     inp != (struct inpcb *)&rawcbtable.inpt_queue;
		     inp = inp->inp_queue.cqe_next) {
			if (!inp->inp_socket || 
			    inp->inp_socket->so_proto->pr_protocol != protocol)
				continue;
			if (inp->inp_faddr.s_addr && 
			    inp->inp_faddr.s_addr != dst.s_addr)
				continue;
			if (inp->inp_secrequire != 0 && 
			    inp->inp_secresult == SR_WAIT)
				break;
		}	    
		if (inp == (struct inpcb *)&rawcbtable.inpt_queue)
			inp = NULL;
	}

#ifdef ENCDEBUG
	if (encdebug && inp != NULL)
		printf("encap: found inp for protocol %d\n", protocol);
#endif /* ENCDEBUG */

	if (inp && inp->inp_secresult == SR_WAIT && inp->inp_secrequire != 0) {
		secrequire = inp->inp_secrequire;
	} else {
		/* 
		 * XXX - is this the right thing to do ?? We need to know if 
		 * IPSec is already in use.
		 * This does only work for host-to-host 
		 */
		flow = find_global_flow(src, altm, dst, altm, 0,0,0);
		if (flow == (struct flow *)NULL)
			return (ENOENT);

		SPI_CHAIN_ATTRIB(secrequire, tdb_onext, flow->flow_sa);
#ifdef ENCDEBUG
		if (encdebug)
			printf("encap: Existing flow (%0x) requires: %d\n", 
			       flow, secrequire);
#endif /* ENCDEBUG */
	}

	if (spi == 0) {
#ifdef ENCDEBUG
		if (encdebug)
			printf("encap: key management failed\n");
#endif
		if (inp != NULL) {
			inp->inp_secresult = SR_FAILED;
			wakeup(inp);
		}
		return (0);
	} else {
		u_int8_t sa_have;

		tdbp = gettdb(spi, dst, sproto);
		if (tdbp == NULL)
			return (ENOENT);
#ifdef ENCDEBUG
		if (encdebug)
			printf("encap: found tdb\n");
#endif /* ENCDEBUG */
		    
		SPI_CHAIN_ATTRIB(sa_have, tdb_onext, tdbp);

		/* Requirements not met */
		if (secrequire & ~sa_have)
			return (EINVAL);
#ifdef ENCDEBUG
		if (encdebug)
			printf("encap: tdb meets requirements\n");
#endif /* ENCDEBUG */

		/*
		 * This is a stupid hack, we do not support socketwise
		 * keying at the moment, so we do it for the whole host
		 */
		error = encap_enable_spi(spi, dst, src, altm, dst, altm,
					 0, 0, 0, sproto, 
					 ENABLE_FLAG_REPLACE|ENABLE_FLAG_LOCAL);

		if (!error) {
#ifdef ENCDEBUG
			if (encdebug)
				printf("encap: key management succeeded\n");
#endif /* ENCDEBUG */
			if (inp != NULL) {
				inp->inp_secresult = SR_SUCCESS;
				wakeup(inp);
			}
		}
	}

	return (error);
}

int
encap_enable_spi(u_int32_t spi, struct in_addr dst,
		 struct in_addr isrc, struct in_addr ismask,
		 struct in_addr idst, struct in_addr idmask,
		 u_int16_t sport, u_int16_t dport,
		 u_int16_t protocol, u_int16_t sproto,
		 u_int16_t flags)
{
	struct sockaddr_encap encapdst, encapgw, encapnetmask;
	struct flow *flow, *flow2, *flow3, *flow4;
	struct in_addr alts, altm;
	struct tdb *tdbp;
	int error = 0;

	tdbp = gettdb(spi, dst, sproto);
	if (tdbp == NULL)
		return (ENOENT);

	bzero((caddr_t) &encapdst, sizeof(struct sockaddr_encap));
	bzero((caddr_t) &encapnetmask, sizeof(struct sockaddr_encap));
	bzero((caddr_t) &encapgw, sizeof(struct sockaddr_encap));

	flow = flow2 = flow3 = flow4 = (struct flow *) NULL;

	/* Retrieve source and destination masks from routing entry */
	if (flags & ENABLE_FLAG_MODIFY) {
		struct route_enc re0, *re = &re0;
		struct sockaddr_encap *dest, *mask;

		bzero((caddr_t) re, sizeof(*re));
		dest = (struct sockaddr_encap *) &re->re_dst;
		dest->sen_family = AF_ENCAP;
		dest->sen_len = SENT_IP4_LEN;
		dest->sen_type = SENT_IP4;
		dest->sen_ip_src = tdbp->tdb_src;
		dest->sen_ip_dst = dst;
		dest->sen_proto = protocol;
		dest->sen_sport = sport;
		dest->sen_dport = dport;
		rtalloc((struct route *) re);
		if (re->re_rt == NULL)
			return (ENOENT);

		mask = (struct sockaddr_encap *) (rt_mask(re->re_rt));
		if (mask == NULL)
			return (ENOENT);

		ismask.s_addr = mask->sen_ip_src.s_addr;
		idmask.s_addr = mask->sen_ip_dst.s_addr;

		RTFREE(re->re_rt);
	}

	isrc.s_addr &= ismask.s_addr;
	idst.s_addr &= idmask.s_addr;

	flow3 = find_global_flow(isrc, ismask, idst, idmask, 
				 protocol, sport, dport);
	if ((flow3 != (struct flow *) NULL) && !(flags & ENABLE_FLAG_REPLACE))
		return (EEXIST);

	/* Check for 0.0.0.0/255.255.255.255 if the flow is local */
	if (flags & ENABLE_FLAG_LOCAL) {
		alts.s_addr = INADDR_ANY;
		altm.s_addr = INADDR_BROADCAST;
		flow4 = find_global_flow(alts, altm, idst, idmask, 
					 protocol, sport, dport);
		if (flow4 != (struct flow *) NULL) {
			if (!(flags & ENABLE_FLAG_REPLACE))
				return (EEXIST);
			else if (flow3 == flow4)
				return (EINVAL);
		}
	}

	flow = get_flow();
	if (flow == (struct flow *) NULL)
		return (ENOBUFS);

	flow->flow_src.s_addr = isrc.s_addr;
	flow->flow_dst.s_addr = idst.s_addr;
	flow->flow_srcmask.s_addr = ismask.s_addr;
	flow->flow_dstmask.s_addr = idmask.s_addr;
	flow->flow_proto = protocol;
	flow->flow_sport = sport;
	flow->flow_dport = dport;

	if (flags & ENABLE_FLAG_LOCAL) {
		flow2 = get_flow();
		if (flow2 == (struct flow *) NULL) {
			FREE(flow, M_TDB);
			return (ENOBUFS);
		}

		flow2->flow_src.s_addr = INADDR_ANY;
		flow2->flow_dst.s_addr = idst.s_addr;
		flow2->flow_srcmask.s_addr = INADDR_BROADCAST;
		flow2->flow_dstmask.s_addr = idmask.s_addr;
		flow2->flow_proto = protocol;
		flow2->flow_sport = sport;
		flow2->flow_dport = dport;

		put_flow(flow2, tdbp);
	}

	put_flow(flow, tdbp);

	/* Setup the encap fields */
	encapdst.sen_len = SENT_IP4_LEN;
	encapdst.sen_family = AF_ENCAP;
	encapdst.sen_type = SENT_IP4;
	encapdst.sen_ip_src.s_addr = flow->flow_src.s_addr;
	encapdst.sen_ip_dst.s_addr = flow->flow_dst.s_addr;
	encapdst.sen_proto = flow->flow_proto;
	encapdst.sen_sport = flow->flow_sport;
	encapdst.sen_dport = flow->flow_dport;

	encapgw.sen_len = SENT_IPSP_LEN;
	encapgw.sen_family = AF_ENCAP;
	encapgw.sen_type = SENT_IPSP;
	encapgw.sen_ipsp_dst.s_addr = tdbp->tdb_dst.s_addr;
	encapgw.sen_ipsp_spi = tdbp->tdb_spi;
	encapgw.sen_ipsp_sproto = tdbp->tdb_sproto;

	encapnetmask.sen_len = SENT_IP4_LEN;
	encapnetmask.sen_family = AF_ENCAP;
	encapnetmask.sen_type = SENT_IP4;
	encapnetmask.sen_ip_src.s_addr = flow->flow_srcmask.s_addr;
	encapnetmask.sen_ip_dst.s_addr = flow->flow_dstmask.s_addr;

	if (flow->flow_proto) {
		encapnetmask.sen_proto = 0xff;

		if (flow->flow_sport)
			encapnetmask.sen_sport = 0xffff;

		if (flow->flow_dport)
			encapnetmask.sen_dport = 0xffff;
	}

	/* If this is set, delete any old route for this flow */
	if (flags & ENABLE_FLAG_REPLACE)
		rtrequest(RTM_DELETE, (struct sockaddr *) &encapdst,
			  (struct sockaddr *) 0,
			  (struct sockaddr *) &encapnetmask, 0,
			  (struct rtentry **) 0);

	/* Add the entry in the routing table */
	error = rtrequest(RTM_ADD, (struct sockaddr *) &encapdst,
			  (struct sockaddr *) &encapgw,
			  (struct sockaddr *) &encapnetmask,
			  RTF_UP | RTF_GATEWAY | RTF_STATIC,
			  (struct rtentry **) 0);
	    
	if (error) {
		encapdst.sen_len = SENT_IP4_LEN;
		encapdst.sen_family = AF_ENCAP;
		encapdst.sen_type = SENT_IP4;
		encapdst.sen_ip_src.s_addr = flow3->flow_src.s_addr;
		encapdst.sen_ip_dst.s_addr = flow3->flow_dst.s_addr;
		encapdst.sen_proto = flow3->flow_proto;
		encapdst.sen_sport = flow3->flow_sport;
		encapdst.sen_dport = flow3->flow_dport;

		encapgw.sen_len = SENT_IPSP_LEN;
		encapgw.sen_family = AF_ENCAP;
		encapgw.sen_type = SENT_IPSP;
		encapgw.sen_ipsp_dst.s_addr = flow3->flow_sa->tdb_dst.s_addr;
		encapgw.sen_ipsp_spi = flow3->flow_sa->tdb_spi;
		encapgw.sen_ipsp_sproto = flow3->flow_sa->tdb_sproto;

		encapnetmask.sen_len = SENT_IP4_LEN;
		encapnetmask.sen_family = AF_ENCAP;
		encapnetmask.sen_type = SENT_IP4;
		encapnetmask.sen_ip_src.s_addr = flow3->flow_srcmask.s_addr;
		encapnetmask.sen_ip_dst.s_addr = flow3->flow_dstmask.s_addr;

		if (flow3->flow_proto) {
			encapnetmask.sen_proto = 0xff;
	    
			if (flow3->flow_sport)
				encapnetmask.sen_sport = 0xffff;
	    
			if (flow->flow_dport)
				encapnetmask.sen_dport = 0xffff;
		}
		
		/* Try to add the old entry back in */
		rtrequest(RTM_ADD, (struct sockaddr *) &encapdst,
			  (struct sockaddr *) &encapgw,
			  (struct sockaddr *) &encapnetmask,
			  RTF_UP | RTF_GATEWAY | RTF_STATIC,
			  (struct rtentry **) 0);
	    
		delete_flow(flow, tdbp);
		if (flow2)
			delete_flow(flow2, tdbp);
		return (error);
	}

	/* If this is a "local" packet flow */
	if (flags & ENABLE_FLAG_LOCAL) {
		encapdst.sen_ip_src.s_addr = INADDR_ANY;
		encapnetmask.sen_ip_src.s_addr = INADDR_BROADCAST;

		if (flags & ENABLE_FLAG_REPLACE)
			rtrequest(RTM_DELETE, (struct sockaddr *) &encapdst,
				  (struct sockaddr *) 0,
				  (struct sockaddr *) &encapnetmask, 0,
				  (struct rtentry **) 0);

		error = rtrequest(RTM_ADD, (struct sockaddr *) &encapdst,
				  (struct sockaddr *) &encapgw,
				  (struct sockaddr *) &encapnetmask,
				  RTF_UP | RTF_GATEWAY | RTF_STATIC,
				  (struct rtentry **) 0);

		if (error) {
				/* Delete the first entry inserted */
			encapdst.sen_ip_src.s_addr = isrc.s_addr;
			encapnetmask.sen_ip_src.s_addr = ismask.s_addr;

			rtrequest(RTM_DELETE, (struct sockaddr *) &encapdst,
				  (struct sockaddr *) 0,
				  (struct sockaddr *) &encapnetmask, 0,
				  (struct rtentry **) 0);

				/* Setup the old entries */
			encapdst.sen_len = SENT_IP4_LEN;
			encapdst.sen_family = AF_ENCAP;
			encapdst.sen_type = SENT_IP4;
			encapdst.sen_ip_src.s_addr = flow3->flow_src.s_addr;
			encapdst.sen_ip_dst.s_addr = flow3->flow_dst.s_addr;
			encapdst.sen_proto = flow3->flow_proto;
			encapdst.sen_sport = flow3->flow_sport;
			encapdst.sen_dport = flow3->flow_dport;

			encapgw.sen_len = SENT_IPSP_LEN;
			encapgw.sen_family = AF_ENCAP;
			encapgw.sen_type = SENT_IPSP;
			encapgw.sen_ipsp_dst.s_addr = flow3->flow_sa->tdb_dst.s_addr;
			encapgw.sen_ipsp_spi = flow3->flow_sa->tdb_spi;
			encapgw.sen_ipsp_sproto = flow3->flow_sa->tdb_sproto;
		   
			encapnetmask.sen_len = SENT_IP4_LEN;
			encapnetmask.sen_family = AF_ENCAP;
			encapnetmask.sen_type = SENT_IP4;
			encapnetmask.sen_ip_src.s_addr = flow3->flow_srcmask.s_addr;
			encapnetmask.sen_ip_dst.s_addr = flow3->flow_dstmask.s_addr;

			if (flow3->flow_proto) {
				encapnetmask.sen_proto = 0xff;
		
				if (flow3->flow_sport)
					encapnetmask.sen_sport = 0xffff;

				if (flow->flow_dport)
					encapnetmask.sen_dport = 0xffff;
			}
		
			rtrequest(RTM_ADD, (struct sockaddr *) &encapdst,
				  (struct sockaddr *) &encapgw,
				  (struct sockaddr *) &encapnetmask,
				  RTF_UP | RTF_GATEWAY | RTF_STATIC,
				  (struct rtentry **) 0);

			encapdst.sen_ip_src.s_addr = INADDR_ANY;
			encapnetmask.sen_ip_src.s_addr = INADDR_BROADCAST;

			rtrequest(RTM_ADD, (struct sockaddr *) &encapdst,
				  (struct sockaddr *) &encapgw,
				  (struct sockaddr *) &encapnetmask,
				  RTF_UP | RTF_GATEWAY | RTF_STATIC,
				  (struct rtentry **) 0);

			delete_flow(flow, tdbp);
			delete_flow(flow2, tdbp);
			return (error);
		}
	}

	/*
	 * If we're here, it means we've successfully added the new
	 * entries, so free the old ones.
	 */
	if (flow3)
		delete_flow(flow3, flow3->flow_sa);

	if (flow4)
		delete_flow(flow4, flow4->flow_sa);
	    
	return 0;
}

int
#ifdef __STDC__
encap_output(struct mbuf *m, ...)
#else
encap_output(m, va_alist)
register struct mbuf *m;
va_dcl
#endif
{
#define SENDERR(e) do { error = e; goto flush;} while (0)
    struct sockaddr_encap encapdst, encapgw, encapnetmask;
    struct flow *flow, *flow2;
    int len, emlen, error = 0;
    struct in_addr alts, altm;
    struct encap_msghdr *emp;
    struct tdb *tdbp, *tdbp2;
    struct expiration *exp;
    caddr_t buffer = 0;
    struct socket *so;
    u_int32_t spi;
    va_list ap;

    va_start(ap, m);
    so = va_arg(ap, struct socket *);
    va_end(ap);

    if ((m == 0) || ((m->m_len < sizeof(int32_t)) &&
		     (m = m_pullup(m, sizeof(int32_t))) == 0))
      return ENOBUFS;

    if ((m->m_flags & M_PKTHDR) == 0)
      panic("encap_output()");

    len = m->m_pkthdr.len; 

    emp = mtod(m, struct encap_msghdr *);

    emlen = emp->em_msglen;
    if (len < emlen)
      SENDERR(EINVAL);

    if (m->m_len < emlen)
    {
	MALLOC(buffer, caddr_t, emlen, M_TEMP, M_WAITOK);	
	if (buffer == 0)
	  SENDERR(ENOBUFS);

	m_copydata(m, 0, emlen, buffer);

	emp = (struct encap_msghdr *) buffer;
    }
	
    if (emp->em_version != PFENCAP_VERSION_1)
      SENDERR(EINVAL);

    bzero((caddr_t) &encapdst, sizeof(struct sockaddr_encap));
    bzero((caddr_t) &encapnetmask, sizeof(struct sockaddr_encap));
    bzero((caddr_t) &encapgw, sizeof(struct sockaddr_encap));

    switch (emp->em_type)
    {
	case EMT_SETSPI:
	    if (emlen <= EMT_SETSPI_FLEN)
	      SENDERR(EINVAL);

	    /* 
	     * If only one of the two outter addresses is set, return
	     * error.
	     */
	    if ((emp->em_osrc.s_addr != 0) ^
		(emp->em_odst.s_addr != 0))
	      SENDERR(EINVAL);	    

	    tdbp = gettdb(emp->em_spi, emp->em_dst, emp->em_sproto);
	    if (tdbp == NULL)
	    {
		MALLOC(tdbp, struct tdb *, sizeof(*tdbp), M_TDB, M_WAITOK);
		if (tdbp == NULL)
		  SENDERR(ENOBUFS);
		
		bzero((caddr_t) tdbp, sizeof(*tdbp));
		
		tdbp->tdb_spi = emp->em_spi;
		tdbp->tdb_dst = emp->em_dst;
		tdbp->tdb_sproto = emp->em_sproto;
		puttdb(tdbp);
	    }
	    else
	    {
		if (tdbp->tdb_xform)
		  (*tdbp->tdb_xform->xf_zeroize)(tdbp);
		
		cleanup_expirations(tdbp->tdb_dst, tdbp->tdb_spi,
				    tdbp->tdb_sproto);
	    }
	    
	    tdbp->tdb_src = emp->em_src;
	    tdbp->tdb_satype = emp->em_satype;

	    /* Check if this is an encapsulating SPI */
	    if (emp->em_osrc.s_addr != 0)
	    {
		tdbp->tdb_flags |= TDBF_TUNNELING;
		tdbp->tdb_osrc = emp->em_osrc;
		tdbp->tdb_odst = emp->em_odst;
		
		/* TTL */
		switch (emp->em_ttl)
		{
		    case IP4_DEFAULT_TTL:
			tdbp->tdb_ttl = 0;
			break;
			
		    case IP4_SAME_TTL:
			tdbp->tdb_flags |= TDBF_SAME_TTL;
			break;

		    default:
			/* Get just the least significant bits */
			tdbp->tdb_ttl = emp->em_ttl % 256;
			break;
		}
	    }
	    
	    /* Clear the INVALID flag */
	    tdbp->tdb_flags &= (~TDBF_INVALID);

	    /* Various timers/counters */
	    if (emp->em_first_use_hard != 0)
	    {
		tdbp->tdb_exp_first_use = emp->em_first_use_hard;
		tdbp->tdb_flags |= TDBF_FIRSTUSE;
	    }
		
	    if (emp->em_first_use_soft != 0)
	    {
		tdbp->tdb_soft_first_use = emp->em_first_use_soft;
		tdbp->tdb_flags |= TDBF_SOFT_FIRSTUSE;
	    }

	    if (emp->em_expire_hard != 0)
	    {
		tdbp->tdb_exp_timeout = emp->em_expire_hard;
		tdbp->tdb_flags |= TDBF_TIMER;
		
		exp = get_expiration();
		if (exp == (struct expiration *) NULL)
		{
		    tdb_delete(tdbp, 0);
		    SENDERR(ENOBUFS);
		}

		exp->exp_dst.s_addr = tdbp->tdb_dst.s_addr;
		exp->exp_spi = tdbp->tdb_spi;
		exp->exp_sproto = tdbp->tdb_sproto;
		exp->exp_timeout = emp->em_expire_hard;
		put_expiration(exp);
	    }
		
	    if (emp->em_expire_soft != 0)
	    {
		tdbp->tdb_soft_timeout = emp->em_expire_soft;
		tdbp->tdb_flags |= TDBF_SOFT_TIMER;

		if (tdbp->tdb_soft_timeout <= tdbp->tdb_exp_timeout)
		{
		    exp = get_expiration();
		    if (exp == (struct expiration *) NULL)
		    {
			tdb_delete(tdbp, 0);
			SENDERR(ENOBUFS);
		    }

		    exp->exp_dst.s_addr = tdbp->tdb_dst.s_addr;
		    exp->exp_spi = tdbp->tdb_spi;
		    exp->exp_sproto = tdbp->tdb_sproto;
		    exp->exp_timeout = emp->em_expire_soft;
		    put_expiration(exp);
		}
	    }
		
	    if (emp->em_bytes_hard != 0)
	    {
		tdbp->tdb_exp_bytes = emp->em_bytes_hard;
		tdbp->tdb_flags |= TDBF_BYTES;
	    }

	    if (emp->em_bytes_soft != 0)
	    {
		tdbp->tdb_soft_bytes = emp->em_bytes_soft;
		tdbp->tdb_flags |= TDBF_SOFT_BYTES;
	    }
		
	    if (emp->em_packets_hard != 0)
	    {
		tdbp->tdb_exp_packets = emp->em_packets_hard;
		tdbp->tdb_flags |= TDBF_PACKETS;
	    }

	    if (emp->em_packets_soft != 0)
	    {
		tdbp->tdb_soft_packets = emp->em_packets_soft;
		tdbp->tdb_flags |= TDBF_SOFT_PACKETS;
	    }

	    error = tdb_init(tdbp, m);
	    if (error)
	    {
		tdb_delete(tdbp, 0);
	      	SENDERR(EINVAL);
	    }
	    
	    break;
		
	case EMT_DELSPI:
	    if (emlen != EMT_DELSPI_FLEN)
	      SENDERR(EINVAL);
	    
	    tdbp = gettdb(emp->em_gen_spi, emp->em_gen_dst, 
			  emp->em_gen_sproto);
	    if (tdbp == NULL)
	      SENDERR(ENOENT);

	    error = tdb_delete(tdbp, 0);
	    if (error)
	      SENDERR(EINVAL);
	    
	    break;

	case EMT_DELSPICHAIN:
	    if (emlen != EMT_DELSPICHAIN_FLEN)
	      SENDERR(EINVAL);

	    tdbp = gettdb(emp->em_gen_spi, emp->em_gen_dst, 
			  emp->em_gen_sproto);
	    if (tdbp == NULL)
	      SENDERR(ENOENT);

	    error = tdb_delete(tdbp, 1);
	    if (error)
	      SENDERR(EINVAL);

	    break;

	case EMT_GRPSPIS:
	    if (emlen != EMT_GRPSPIS_FLEN)
	      SENDERR(EINVAL);
	    
	    tdbp = gettdb(emp->em_rel_spi, emp->em_rel_dst, 
			  emp->em_rel_sproto);
	    if (tdbp == NULL)
	      SENDERR(ENOENT);

	    tdbp2 = gettdb(emp->em_rel_spi2, emp->em_rel_dst2,
			   emp->em_rel_sproto2);
	    if (tdbp2 == NULL)
	      SENDERR(ENOENT);
	    
	    tdbp->tdb_onext = tdbp2;
	    tdbp2->tdb_inext = tdbp;
	    
	    error = 0;

	    break;

	case EMT_RESERVESPI:
	    if (emlen != EMT_RESERVESPI_FLEN)
	      SENDERR(EINVAL);
	    
	    spi = reserve_spi(emp->em_gen_spi, emp->em_gen_dst, 
			      emp->em_gen_sproto, &error);
	    if (spi == 0)
	      SENDERR(error);

	    emp->em_gen_spi = spi;
	    
	    /* If we're using a buffer, copy the data back to an mbuf. */
	    if (buffer)
	      m_copyback(m, 0, emlen, buffer);

	    /* Send it back to us */
	    if (sbappendaddr(&so->so_rcv, &encap_src, m,
			     (struct mbuf *) 0) == 0)
	      SENDERR(ENOBUFS);
	    else
	      sorwakeup(so);		/* wakeup  */

	    m = NULL;			/* So it's not free'd */
	    error = 0;
	    
	    break;
	    
	case EMT_ENABLESPI:
	    if (emlen != EMT_ENABLESPI_FLEN)
	      SENDERR(EINVAL);

	    error = encap_enable_spi(emp->em_ena_spi, emp->em_ena_dst,
				     emp->em_ena_isrc, emp->em_ena_ismask,
				     emp->em_ena_idst, emp->em_ena_idmask,
				     emp->em_ena_sport, emp->em_ena_dport,
				     emp->em_ena_protocol, emp->em_ena_sproto,
				     emp->em_ena_flags);

	    break;

	case EMT_DISABLESPI:
	    if (emlen != EMT_DISABLESPI_FLEN)
	      SENDERR(EINVAL);

            tdbp = gettdb(emp->em_ena_spi, emp->em_ena_dst, 
			  emp->em_ena_sproto);
            if (tdbp == NULL)
              SENDERR(ENOENT);

	    /* Retrieve source and destination masks from routing entry */
	    if (emp->em_ena_flags & ENABLE_FLAG_MODIFY) {
		    struct route_enc re0, *re = &re0;
		    struct sockaddr_encap *dest, *mask;
	      
		    bzero((caddr_t) re, sizeof(*re));
		    dest = (struct sockaddr_encap *) &re->re_dst;
		    dest->sen_family = AF_ENCAP;
		    dest->sen_len = SENT_IP4_LEN;
		    dest->sen_type = SENT_IP4;
		    dest->sen_ip_src = tdbp->tdb_src;
		    dest->sen_ip_dst = emp->em_ena_dst;
		    dest->sen_proto = emp->em_ena_protocol;
		    dest->sen_sport = emp->em_ena_sport;
		    dest->sen_dport = emp->em_ena_dport;
		    rtalloc((struct route *) re);
		    if (re->re_rt == NULL)
			    return (ENOENT);

		    mask = (struct sockaddr_encap *) (rt_mask(re->re_rt));
		    if (mask == NULL)
			    return (ENOENT);

		    emp->em_ena_ismask.s_addr = mask->sen_ip_src.s_addr;
		    emp->em_ena_idmask.s_addr = mask->sen_ip_dst.s_addr;

		    RTFREE(re->re_rt);
	    }

	    emp->em_ena_isrc.s_addr &= emp->em_ena_ismask.s_addr;
	    emp->em_ena_idst.s_addr &= emp->em_ena_idmask.s_addr;

	    flow = find_flow(emp->em_ena_isrc, emp->em_ena_ismask,
			     emp->em_ena_idst, emp->em_ena_idmask,
			     emp->em_ena_protocol, emp->em_ena_sport,
			     emp->em_ena_dport, tdbp);
	    if (flow == (struct flow *) NULL)
	      SENDERR(ENOENT);

	    if (emp->em_ena_flags & ENABLE_FLAG_LOCAL)
	    {
		alts.s_addr = INADDR_ANY;
		altm.s_addr = INADDR_BROADCAST;

		flow2 = find_flow(alts, altm, emp->em_ena_idst,
				  emp->em_ena_idmask, emp->em_ena_protocol,
				  emp->em_ena_sport, emp->em_ena_dport, tdbp);
		if (flow2 == (struct flow *) NULL)
		  SENDERR(ENOENT);

		if (flow == flow2)
		  SENDERR(EINVAL);
	    }

            /* Setup the encap fields */
            encapdst.sen_len = SENT_IP4_LEN;
            encapdst.sen_family = AF_ENCAP;
            encapdst.sen_type = SENT_IP4;
            encapdst.sen_ip_src.s_addr = flow->flow_src.s_addr;
            encapdst.sen_ip_dst.s_addr = flow->flow_dst.s_addr;
            encapdst.sen_proto = flow->flow_proto;
            encapdst.sen_sport = flow->flow_sport;
            encapdst.sen_dport = flow->flow_dport;

            encapnetmask.sen_len = SENT_IP4_LEN;
            encapnetmask.sen_family = AF_ENCAP;
            encapnetmask.sen_type = SENT_IP4;
            encapnetmask.sen_ip_src.s_addr = flow->flow_srcmask.s_addr;
            encapnetmask.sen_ip_dst.s_addr = flow->flow_dstmask.s_addr;

            if (flow->flow_proto)
            {
                encapnetmask.sen_proto = 0xff;

                if (flow->flow_sport)
                  encapnetmask.sen_sport = 0xffff;

                if (flow->flow_dport)
                  encapnetmask.sen_dport = 0xffff;
            }

            /* Delete the entry */
            rtrequest(RTM_DELETE, (struct sockaddr *) &encapdst,
		      (struct sockaddr *) 0,
		      (struct sockaddr *) &encapnetmask, 0,
		      (struct rtentry **) 0);

	    if (emp->em_ena_flags & ENABLE_FLAG_MODIFY) {
		    encapgw.sen_len = SENT_IPSP_LEN;
		    encapgw.sen_family = AF_ENCAP;
		    encapgw.sen_type = SENT_IPSP;
		    encapgw.sen_ipsp_dst.s_addr = emp->em_ena_dst.s_addr;
		    encapgw.sen_ipsp_spi = htonl(1);
		    encapgw.sen_ipsp_sproto = IPPROTO_ESP;
		    error = rtrequest(RTM_ADD, (struct sockaddr *) &encapdst,
				      (struct sockaddr *) &encapgw,
				      (struct sockaddr *) &encapnetmask,
				      RTF_UP | RTF_GATEWAY | RTF_STATIC,
				      (struct rtentry **) 0);
	    }
	    
	    if (emp->em_ena_flags & ENABLE_FLAG_LOCAL)
	    {

		encapdst.sen_ip_src.s_addr = INADDR_ANY;
		encapnetmask.sen_ip_src.s_addr = INADDR_BROADCAST;

		rtrequest(RTM_DELETE, (struct sockaddr *) &encapdst,
			  (struct sockaddr *) 0,
			  (struct sockaddr *) &encapnetmask, 0,
			  (struct rtentry **) 0);
		
		if (emp->em_ena_flags & ENABLE_FLAG_MODIFY) {
			encapgw.sen_len = SENT_IPSP_LEN;
			encapgw.sen_family = AF_ENCAP;
			encapgw.sen_type = SENT_IPSP;
			encapgw.sen_ipsp_dst.s_addr = emp->em_ena_dst.s_addr;
			encapgw.sen_ipsp_spi = htonl(1);
			encapgw.sen_ipsp_sproto = IPPROTO_ESP;
			error = rtrequest(RTM_ADD, 
					  (struct sockaddr *) &encapdst,
					  (struct sockaddr *) &encapgw,
					  (struct sockaddr *) &encapnetmask,
					  RTF_UP | RTF_GATEWAY | RTF_STATIC,
					  (struct rtentry **) 0);
		}
		delete_flow(flow2, tdbp);
	    }

	    delete_flow(flow, tdbp);

	    break;

	case EMT_REPLACESPI:
	    if (emlen <= EMT_REPLACESPI_FLEN)
	      SENDERR(EINVAL);
	    
	    /* XXX Not yet finished */

	    SENDERR(EINVAL);
	    
	    break;
	    
	case EMT_NOTIFY:
	    if (emlen < EMT_NOTIFY_FLEN)
	      SENDERR(EINVAL);

	    if (emp->em_not_type != NOTIFY_REQUEST_SA)
	      SENDERR(EINVAL);

	    error = encap_notify_sa(emp->em_not_spi, 
				    emp->em_not_dst, emp->em_not_src, 
				    emp->em_not_sport, emp->em_not_dport,
				    emp->em_not_protocol, emp->em_not_sproto);

	    break;
	    
	default:
	    SENDERR(EINVAL);
    }
    
flush:
    if (m)
      m_freem(m);

    if (buffer)
      free(buffer, M_TEMP);

    return error;
}

void
encap_sendnotify(int subtype, struct tdb *tdbp, void *data)
{
    struct encap_msghdr em;
    struct mbuf *m;
    
    bzero(&em, sizeof(struct encap_msghdr));

    em.em_msglen = EMT_NOTIFY_FLEN;
    em.em_version = PFENCAP_VERSION_1;
    em.em_type = EMT_NOTIFY;
    
    notify_msgids++;

    switch (subtype)
    {
	case NOTIFY_SOFT_EXPIRE:
	case NOTIFY_HARD_EXPIRE:
	    em.em_not_spi = tdbp->tdb_spi;
	    em.em_not_sproto = tdbp->tdb_sproto;
	    em.em_not_dst.s_addr = tdbp->tdb_dst.s_addr;
	    em.em_not_type = subtype;
	    em.em_not_satype = tdbp->tdb_satype;
	    break;
	    
	case NOTIFY_REQUEST_SA:
	    em.em_not_dst.s_addr = tdbp->tdb_dst.s_addr;
#ifdef INET
	    if (data != NULL) {
		    struct inpcb *inp = (struct inpcb *) data;
		    struct socket *so = inp->inp_socket;
		    em.em_not_dport = inp->inp_fport;
		    em.em_not_sport = inp->inp_lport;
		    if (so != 0) 
			    em.em_not_protocol = so->so_proto->pr_protocol;
	    }
#endif
	    em.em_not_type = subtype;
	    em.em_not_satype = tdbp->tdb_satype;
	    break;

	default:
#ifdef ENCDEBUG
	    if (encdebug)
	        log(LOG_WARNING, "encap_sendnotify(): unknown subtype %d\n", subtype);
#endif /* ENCDEBUG */
	    return;
    }

    m = m_gethdr(M_DONTWAIT, MT_DATA);
    if (m == NULL)
    {
	if (encdebug)
	  log(LOG_ERR, "encap_sendnotify(): m_gethdr() returned NULL\n");
	return;
    }
   
    m->m_len = min(MLEN, em.em_msglen); 
    m_copyback(m, 0, em.em_msglen, (caddr_t) &em);
    raw_input(m, &encap_proto, &encap_src, &encap_dst);

    return;
}

struct ifaddr *
encap_findgwifa(struct sockaddr *gw)
{
    return enc_softc.if_addrlist.tqh_first;
}
