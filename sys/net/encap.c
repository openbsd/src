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

extern struct ifnet loif;

extern int ipspkernfs_dirty;

void encap_init(void);
int encap_output __P((struct mbuf *, ...));
int encap_usrreq(struct socket *, int, struct mbuf *, struct mbuf *, struct mbuf *);

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
encap_usrreq(register struct socket *so, int req, struct mbuf *m, struct mbuf *nam, struct mbuf *control)
{
	register int error = 0;
	register struct rawcb *rp = sotorawcb(so);
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
	struct socket *so;
	int len, emlen, error = 0, nspis, i;
	struct encap_msghdr *emp;
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct sockaddr_encap *sen, *sen2;
	struct sockaddr_in *sin;
	struct tdb *tdbp, *tprev;
	va_list ap;

	va_start(ap, m);
	so = va_arg(ap, struct socket *);
	va_end(ap);

	if ((m == 0) || ((m->m_len < sizeof(long)) &&
			 (m = m_pullup(m, sizeof(long))) == 0))
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
		m = m_pullup(m, emlen);
		if (m == NULL)
		  SENDERR(ENOBUFS);
		
		emp = mtod(m, struct encap_msghdr *);
	}
	
	switch (emp->em_type)
	{
	      case EMT_IFADDR:
		/*
		 * Set the default source address for an encap interface
		 */

		ifp = &(enc_softc[emp->em_ifn].enc_if);
		
		if ((ifp->if_addrlist.tqh_first == NULL) ||
		    (ifp->if_addrlist.tqh_first->ifa_addr == NULL) ||
		    (ifp->if_addrlist.tqh_first->ifa_addr->sa_family != AF_ENCAP))
		{
			MALLOC(ifa, struct ifaddr *, sizeof (struct ifaddr) + 2*SENT_DEFIF_LEN, M_IFADDR, M_WAITOK);
			if (ifa == NULL)
			  SENDERR(ENOBUFS);
			bzero((caddr_t)ifa, sizeof (struct ifaddr) + 2*SENT_DEFIF_LEN);
			sen = (struct sockaddr_encap *)(ifa + 1);
			sen2 = (struct sockaddr_encap *)((caddr_t)sen + SENT_DEFIF_LEN);
			ifa->ifa_addr = (struct sockaddr *)sen;
			ifa->ifa_dstaddr = (struct sockaddr *)sen2;
			ifa->ifa_ifp = ifp;
			TAILQ_INSERT_HEAD(&(ifp->if_addrlist), ifa, ifa_list);
		}
		else
		{
			sen = (struct sockaddr_encap *)((&(ifp->if_addrlist))->tqh_first->ifa_addr);
			sen2 = (struct sockaddr_encap *)((&(ifp->if_addrlist))->tqh_first->ifa_dstaddr);
		}

		sen->sen_family = AF_ENCAP;
		sen->sen_len = SENT_DEFIF_LEN;
		sen->sen_type = SENT_DEFIF;
		sin = (struct sockaddr_in *) &(sen->sen_dfl);
		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		sin->sin_addr = emp->em_ifa;

		*sen2 = *sen;

		break;
		
	      case EMT_SETSPI:
		if (emp->em_if >= nencap)
		  SENDERR(ENODEV);
		
		tdbp = gettdb(emp->em_spi, emp->em_dst);
		if (tdbp == NULL)
		{
			MALLOC(tdbp, struct tdb *, sizeof (*tdbp), M_TDB, M_WAITOK);
			if (tdbp == NULL)
			  SENDERR(ENOBUFS);
			
			bzero((caddr_t)tdbp, sizeof(*tdbp));
			
			tdbp->tdb_spi = emp->em_spi;
			tdbp->tdb_dst = emp->em_dst;
			tdbp->tdb_rcvif = &(enc_softc[emp->em_if].enc_if);
			puttdb(tdbp);
		}
		else
		  (*tdbp->tdb_xform->xf_zeroize)(tdbp);

		error = tdb_init(tdbp, m);
		ipspkernfs_dirty = 1;
		break;
		
	      case EMT_DELSPI:
		if (emp->em_if >= nencap)
		  SENDERR(ENODEV);

		tdbp = gettdb(emp->em_spi, emp->em_dst);
		if (tdbp == NULL)
		{
			error = EINVAL;
			break;
		}

		if (emp->em_alg != tdbp->tdb_xform->xf_type)
		{
			error = EINVAL;
			break;
		}

		error = tdb_delete(tdbp, 0);
		break;

	      case EMT_DELSPICHAIN:
                if (emp->em_if >= nencap)
                  SENDERR(ENODEV);

                tdbp = gettdb(emp->em_spi, emp->em_dst);
                if (tdbp == NULL)
                {
                        error = EINVAL;
                        break;
                }

		if (emp->em_alg != tdbp->tdb_xform->xf_type)
		{
			error = EINVAL;
			break;
		}

                error = tdb_delete(tdbp, 1);
                break;

	      case EMT_GRPSPIS:
		nspis = (emlen - 4) / 12;
		if (nspis * 12 + 4 != emlen)
		{
			SENDERR(EINVAL);
			break;
		}
		
		for (i = 0; i < nspis; i++)
		  if ((tdbp = gettdb(emp->em_rel[i].emr_spi, emp->em_rel[i].emr_dst)) == NULL)
		    SENDERR(ENOENT);
		  else
		    emp->em_rel[i].emr_tdb = tdbp;
		tprev = emp->em_rel[0].emr_tdb;
		tprev->tdb_inext = NULL;
		for (i = 1; i < nspis; i++)
		{
			tdbp = emp->em_rel[i].emr_tdb;
			tprev->tdb_onext = tdbp;
			tdbp->tdb_inext = tprev;
			tprev = tdbp;
		}
		tprev->tdb_onext = NULL;
		ipspkernfs_dirty = 1;
		error = 0;
		break;

	      default:
		SENDERR(EINVAL);
	}
	
	return error;

      flush:
	if (m)
	  m_freem(m);
	return error;
}

struct ifaddr *
encap_findgwifa(struct sockaddr *gw)
{
	struct sockaddr_encap *egw = (struct sockaddr_encap *)gw;
	u_char *op = (u_char *)gw;
	int i, j;
	struct ifaddr *retval = loif.if_addrlist.tqh_first;
	union
	{
		struct in_addr ia;
		u_char io[4];
	} iao;
	
	switch (egw->sen_type)
	{
	      case SENT_IPSP:
		return enc_softc[egw->sen_ipsp_ifn].enc_if.if_addrlist.tqh_first;
		break;
		
	      case SENT_IP4:
		/*
		 * Pretty-much standard options walking code.
		 * Repeated elsewhere as necessary
		 */

		for (i = SENT_IP4_LEN; i < egw->sen_len;)
		  switch (op[i])
		  {
			case SENO_EOL:
			  goto opt_done;
			  
			case SENO_NOP:
			  i++;
			  continue;
			  
			case SENO_IFN:
			  if (op[i+1] != 3)
			  {
				  return NULL;
			  }
			  retval = enc_softc[op[i+2]].enc_if.if_addrlist.tqh_first;
			  goto opt_done;
			  
			case SENO_IFIP4A:
			  if (op[i+1] != 6) /* XXX -- IPv4 address */
			  {
				  return NULL;
			  }
			  iao.io[0] = op[i+2];
			  iao.io[1] = op[i+3];
			  iao.io[2] = op[i+4];
			  iao.io[3] = op[i+5];

			  for (j = 0; j < nencap; j++)
			  {
				  struct ifaddr *ia = (struct ifaddr *)enc_softc[j].enc_if.if_addrlist.tqh_first;
				  
				  struct sockaddr_in *si = (struct sockaddr_in *)ia->ifa_addr;
				  
				  if ((si->sin_family == AF_INET) && (si->sin_addr.s_addr == iao.ia.s_addr))
				  {
					  retval = ia;
					  goto opt_done;
				  }
			  }
			  i += 6;
			  break;
			  
			default:
			  if (op[i+1] == 0)
			    return NULL;
			  i += op[i+i];
		  }
	      opt_done:
		break;
	}
	return retval;
}
