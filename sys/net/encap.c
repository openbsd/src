/*	$OpenBSD: encap.c,v 1.7 1997/07/02 06:58:40 provos Exp $	*/

/*
 * The author of this code is John Ioannidis, ji@tla.org,
 * 	(except when noted otherwise).
 *
 * This code was written for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis, kermit@forthnet.gr.
 *
 * Copyright (C) 1995, 1996, 1997 by John Ioannidis and Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NEITHER AUTHOR MAKES ANY
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

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>
#include <machine/stdarg.h>

#ifdef INET
#include <netinet/in.h>
#endif 

#include <net/encap.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_ip4.h>

extern struct ifnet loif;

extern int ipspkernfs_dirty;

void encap_init(void);
int encap_output __P((struct mbuf *, ...));
int encap_usrreq(struct socket *, int, struct mbuf *, struct mbuf *, 
		 struct mbuf *);

extern int tdb_init(struct tdb *, struct mbuf *);

extern struct domain encapdomain;

struct	sockaddr encap_dst = { 2, PF_ENCAP, };
struct	sockaddr encap_src = { 2, PF_ENCAP, };
struct	sockproto encap_proto = { PF_ENCAP, };

struct protosw encapsw[] = { 
    { SOCK_RAW,	&encapdomain,	0,		PR_ATOMIC|PR_ADDR,
      raw_input,	encap_output,	raw_ctlinput,	0,
      encap_usrreq,
      encap_init,	0,		0,		0,
    },
};

struct domain encapdomain =
{ AF_ENCAP, "encapsulation", 0, 0, 0, 
  encapsw, &encapsw[sizeof(encapsw)/sizeof(encapsw[0])], 0,
  rn_inithead, 16, sizeof(struct sockaddr_encap)};


void
encap_init()
{
    struct xformsw *xsp;
    
    for (xsp = xformsw; xsp < xformswNXFORMSW; xsp++)
    {
	printf("encap_init: attaching <%s>\n", xsp->xf_name);
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
	if ((so->so_pcb = (caddr_t)rp))
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
	    free((caddr_t)rp, M_PCB);
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
#ifdef __STDC__
encap_output(struct mbuf *m, ...)
#else
encap_output(m, va_alist)
register struct mbuf *m;
va_dcl
#endif
{
#define SENDERR(e) do { error = e; goto flush;} while (0)
    int len, emlen, error = 0;
    struct encap_msghdr *emp;
    struct tdb *tdbp, *tdbp2;
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
      panic("encap_output");

    len = m->m_pkthdr.len; 

    emp = mtod(m, struct encap_msghdr *);

    emlen = emp->em_msglen;
    if ((len < emlen))
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

	    tdbp = gettdb(emp->em_spi, emp->em_dst);
	    if (tdbp == NULL)
	    {
		MALLOC(tdbp, struct tdb *, sizeof (*tdbp), M_TDB, M_WAITOK);
		if (tdbp == NULL)
		  SENDERR(ENOBUFS);
		
		bzero((caddr_t)tdbp, sizeof(*tdbp));
		
		tdbp->tdb_spi = emp->em_spi;
		tdbp->tdb_dst = emp->em_dst;

		puttdb(tdbp);
	    }
	    else
	      if (tdbp->tdb_xform)
	        (*tdbp->tdb_xform->xf_zeroize)(tdbp);
	    
	    tdbp->tdb_proto = emp->em_proto;
	    tdbp->tdb_sport = emp->em_sport;
	    tdbp->tdb_dport = emp->em_dport;

	    tdbp->tdb_src = emp->em_src;

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
	    if (emp->em_relative_hard != 0)
	    {
		tdbp->tdb_exp_relative = emp->em_relative_hard;
		tdbp->tdb_flags |= TDBF_RELATIVE;
	    }

	    if (emp->em_relative_soft != 0)
	    {
		tdbp->tdb_soft_relative = emp->em_relative_soft;
		tdbp->tdb_flags |= TDBF_SOFT_RELATIVE;
	    }
		
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
	    }
		
	    if (emp->em_expire_soft != 0)
	    {
		tdbp->tdb_soft_timeout = emp->em_expire_soft;
		tdbp->tdb_flags |= TDBF_SOFT_TIMER;
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
	      SENDERR(EINVAL);
	    
	    ipspkernfs_dirty = 1;

	    break;
		
	case EMT_DELSPI:
	    if (emlen != EMT_DELSPI_FLEN)
	      SENDERR(EINVAL);
	    
	    tdbp = gettdb(emp->em_gen_spi, emp->em_gen_dst);
	    if (tdbp == NULL)
	      SENDERR(ENOENT);
	    
	    error = tdb_delete(tdbp, 0);
	    if (error)
	      SENDERR(EINVAL);
	    
	    break;

	case EMT_DELSPICHAIN:
	    if (emlen != EMT_DELSPICHAIN_FLEN)
	      SENDERR(EINVAL);

	    tdbp = gettdb(emp->em_gen_spi, emp->em_gen_dst);
	    if (tdbp == NULL)
	      SENDERR(ENOENT);
	    
	    error = tdb_delete(tdbp, 1);
	    if (error)
	      SENDERR(EINVAL);

	    break;

	case EMT_GRPSPIS:
	    if (emlen != EMT_GRPSPIS_FLEN)
	      SENDERR(EINVAL);
	    
	    tdbp = gettdb(emp->em_rel_spi, emp->em_rel_dst);
	    if (tdbp == NULL)
	      SENDERR(ENOENT);

	    tdbp2 = gettdb(emp->em_rel_spi2, emp->em_rel_dst2);
	    if (tdbp2 == NULL)
	      SENDERR(ENOENT);
	    
	    tdbp->tdb_onext = tdbp2;
	    tdbp2->tdb_inext = tdbp;
	    
	    ipspkernfs_dirty = 1;
	    error = 0;
	    break;

	case EMT_RESERVESPI:
	    if (emlen != EMT_RESERVESPI_FLEN)
	      SENDERR(EINVAL);
	    
	    spi = reserve_spi(emp->em_gen_spi, emp->em_gen_dst, &error);
	    if (spi == 0)
	      SENDERR(error);

	    emp->em_gen_spi = spi;
	    
	    /* If we're using a buffer, copy the data back to the mbuf */
	    if (buffer)
	      m_copyback(m, 0, emlen, buffer);

	    /* Send it back to us */
	    if (sbappendaddr(&so->so_rcv, &encap_src, m, 
			     (struct mbuf *)0) == 0)
	      SENDERR(ENOBUFS);
	    else
	      sorwakeup(so);		/* wakeup  */

	    error = 0;
	    
	    break;
	    
	case EMT_ENABLESPI:
	    if (emlen != EMT_ENABLESPI_FLEN)
	      SENDERR(EINVAL);
	    
	    tdbp = gettdb(emp->em_gen_spi, emp->em_gen_dst);
	    if (tdbp == NULL)
	      SENDERR(ENOENT);

	    /* Clear the INVALID flag */
	    tdbp->tdb_flags &= (~TDBF_INVALID);

	    /* XXX Install a routing entry */

	    error = 0;

	    break;
	    
	case EMT_DISABLESPI:
	    if (emlen != EMT_DISABLESPI_FLEN)
	      SENDERR(EINVAL);
	    
	    tdbp = gettdb(emp->em_gen_spi, emp->em_gen_dst);
	    if (tdbp == NULL)
	      SENDERR(ENOENT);
	    
	    /* Set the INVALID flag */
	    tdbp->tdb_flags |= TDBF_INVALID;

	    /* XXX Delete a routing entry, if on exists */

	    error = 0;
	    
	    break;

	case EMT_NOTIFY:
	    if (emlen <= EMT_NOTIFY_FLEN)
	      SENDERR(EINVAL);
	    
	    /* XXX Not yet finished */

	    SENDERR(EINVAL);

	    break;
	    
	default:
	    SENDERR(EINVAL);
    }
    
    if (buffer)
      free(buffer, M_TEMP);

    return error;
    
flush:
    if (m)
      m_freem(m);

    if (buffer)
      free(buffer, M_TEMP);

    return error;
}

struct ifaddr *
encap_findgwifa(struct sockaddr *gw)
{
    return enc_softc.if_addrlist.tqh_first;
}
