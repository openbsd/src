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

/*
 * DES-CBC
 * Per RFC1829 (Metzger & Simpson, 1995)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <machine/cpu.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#include <sys/socketvar.h>
#include <net/raw_cb.h>
#include <net/encap.h>

#include <netinet/ip_icmp.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>
#include <dev/rndvar.h>

extern struct ifnet loif;

extern void des_ecb_encrypt(caddr_t, caddr_t, caddr_t, int);
extern void des_set_key(caddr_t, caddr_t);

extern MD5_CTX IP4_ctx;

extern int ticks;
#ifdef EXT_CLOCK
extern int clock_count;
#endif

int
espdes_attach()
{
	return 0;
}

/*
 * espdes_init() is called when an SPI is being set up. It interprets the
 * encap_msghdr present in m, and sets up the transformation data, in
 * this case, the encryption and decryption key schedules
 */

int
espdes_init(struct tdb *tdbp, struct xformsw *xsp, struct mbuf *m)
{
	struct espdes_xdata *xd;
	struct encap_msghdr *em;
	u_long rk[2];

	tdbp->tdb_xform = xsp;

	m = m_pullup(m, ESP_ULENGTH);
	if (m == NULL)
	{
#ifdef ENCDEBUG
		if (encdebug)
		  printf("espdes_init: can't pull up %d bytes\n", ESP_ULENGTH);
#endif ENCDEBUG
		return ENOBUFS;
	}

	MALLOC(tdbp->tdb_xdata, caddr_t, sizeof (struct espdes_xdata),
	       M_XDATA, M_WAITOK);
	if (tdbp->tdb_xdata == NULL)
	  return ENOBUFS;
	bzero(tdbp->tdb_xdata, sizeof (struct espdes_xdata));
	xd = (struct espdes_xdata *)tdbp->tdb_xdata;

	em = mtod(m, struct encap_msghdr *);
	if (em->em_msglen - EMT_SETSPI_FLEN != ESP_ULENGTH)
	{
		free((caddr_t)tdbp->tdb_xdata, M_XDATA);
		tdbp->tdb_xdata = NULL;
		return EINVAL;
	}

	m_copydata(m, EMT_SETSPI_FLEN, ESP_ULENGTH, (caddr_t)xd);

	rk[0] = xd->edx_eks[0][0];	/* some overloading doesn't hurt */
	rk[1] = xd->edx_eks[0][1];	/* XXX -- raw-major order */

	des_set_key((caddr_t)rk, (caddr_t)(xd->edx_eks));
	rk[0] = rk[1] = 0;		/* zeroize! */

	return 0;
}

int
espdes_zeroize(struct tdb *tdbp)
{
	FREE(tdbp->tdb_xdata, M_XDATA);
	return 0;
}


struct mbuf *
espdes_input(struct mbuf *m, struct tdb *tdb)
{
	struct espdes_xdata *xd;
	struct ip *ip, ipo;
	u_char iv[8], niv[8], blk[8];
	u_char *idat, *odat;
	struct esp *esp;
	struct ifnet *rcvif;
	int ohlen, plen, ilen, olen, i;
	struct mbuf *mi, *mo;

	xd = (struct espdes_xdata *)tdb->tdb_xdata;
	ohlen = sizeof (struct ip) + ESP_FLENGTH;

	rcvif = m->m_pkthdr.rcvif;
	if (rcvif == NULL)
	{
#ifdef ENCDEBUG
		if (encdebug)
		  printf("espdes_input: receive interface is NULL!!!\n");
#endif ENCDEBUG
		rcvif = &loif;
	}

	ip = mtod(m, struct ip *);
	ipo = *ip;
	esp = (struct esp *)(ip + 1);

	plen = m->m_pkthdr.len - sizeof (struct ip) - sizeof (u_long) - xd->edx_ivlen;
	if (plen & 07)
	{
#ifdef ENCDEBUG
		if (encdebug)
		  printf("espdes_input: payload not a multiple of 8 octets\n");
#endif ENCDEBUG
		espstat.esps_badilen++;
		m_freem(m);
		return NULL;
	}

	ilen = m->m_len - sizeof (struct ip) - 8;
	idat = mtod(m, unsigned char *) + sizeof (struct ip) + 8;

	iv[0] = esp->esp_iv[0];
	iv[1] = esp->esp_iv[1];
	iv[2] = esp->esp_iv[2];
	iv[3] = esp->esp_iv[3];
	if (xd->edx_ivlen == 4)
	{
		iv[4] = ~esp->esp_iv[0];
		iv[5] = ~esp->esp_iv[1];
		iv[6] = ~esp->esp_iv[2];
		iv[7] = ~esp->esp_iv[3];
	}
	else
	{
		iv[4] = esp->esp_iv[4];
		iv[5] = esp->esp_iv[5];
		iv[6] = esp->esp_iv[6];
		iv[7] = esp->esp_iv[7];

		ilen -= 4;
		idat += 4;
	}

	olen = ilen;
	odat = idat;
	mi = mo = m;
	i = 0;

	/*
	 * At this point:
	 *   plen is # of encapsulated payload octets
	 *   ilen is # of octets left in this mbuf
	 *   idat is first encapsulated payload octed in this mbuf
	 *   same for olen and odat
	 *   iv contains the IV.
	 *   mi and mo point to the first mbuf
	 *
	 * From now on until the end of the mbuf chain:
	 *   . move the next eight octets of the chain into blk[]
	 *     (ilen, idat, and mi are adjusted accordingly)
	 *     and save it back into iv[]
	 *   . decrypt blk[], xor with iv[], put back into chain
	 *     (olen, odat, amd mo are adjusted accordingly)
	 *   . repeat
	 */

	while (plen > 0)		/* while not done */
	{
		while (ilen == 0)	/* we exhausted previous mbuf */
		{
			mi = mi->m_next;
			if (mi == NULL)
			  panic("espdes_input: bad chain (i)\n");
			ilen = mi->m_len;
			idat = (u_char *)mi->m_data;
		}

		blk[i] = niv[i] = *idat++;
		i++;
		ilen--;

		if (i == 8)
		{
			des_ecb_encrypt(blk, blk, (caddr_t)(xd->edx_eks), 0);
			for (i=0; i<8; i++)
			{
				while (olen == 0)
				{
					mo = mo->m_next;
					if (mo == NULL)
					  panic("espdes_input: bad chain (o)\n");
					olen = mo->m_len;
					odat = (u_char *)mo->m_data;
				}
				*odat = blk[i] ^ iv[i];
				iv[i] = niv[i];
				blk[i] = *odat++; /* needed elsewhere */
				olen--;
			}
			i = 0;
		}

		plen--;
	}

	/*
	 * Now, the entire chain has been decrypted. As a side effect,
	 * blk[7] contains the next protocol, and blk[6] contains the
	 * amount of padding the original chain had. Chop off the
	 * appropriate parts of the chain, and return.
	 */

	m_adj(m, -blk[6] - 2);
	m_adj(m, 4 + xd->edx_ivlen);
	if (m->m_len < sizeof (struct ip))
	{
		m = m_pullup(m, sizeof (struct ip));
		if (m == NULL)
		{
			return NULL;
		}
	}

	ip = mtod(m, struct ip *);
	ipo.ip_p = blk[7];
	ipo.ip_id = htons(ipo.ip_id);
	ipo.ip_off = 0;
	ipo.ip_len += sizeof (struct ip) - 4 - xd->edx_ivlen - blk[6] - 2;
	ipo.ip_len = htons(ipo.ip_len);
	ipo.ip_sum = 0;
	*ip = ipo;
	ip->ip_sum = in_cksum(m, sizeof (struct ip));

	return m;
}

int
espdes_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb, struct mbuf **mp)
{
	struct espdes_xdata *xd;
	struct ip *ip, ipo;
	int i, ilen, olen, ohlen, nh, rlen, plen, padding;
	u_long spi;
	struct mbuf *mi, *mo;
	u_char *pad, *idat, *odat;
	u_char iv[8], blk[8];


	m = m_pullup(m, sizeof (struct ip));
	if (m == NULL)
	  return ENOBUFS;

	ip = mtod(m, struct ip *);
	spi = tdb->tdb_spi;
	
	xd = (struct espdes_xdata *)tdb->tdb_xdata;
	ilen = ntohs(ip->ip_len);
	ohlen = sizeof (u_long) + xd->edx_ivlen;

	ipo = *ip;
	nh = ipo.ip_p;

	rlen = ilen - sizeof (struct ip); /* raw payload length  */
        padding = ((8 - ((rlen + 2) % 8)) % 8) + 2;

	pad = (u_char *)m_pad(m, padding);
	if (pad == NULL)
	  return ENOBUFS;

	pad[padding-2] = padding - 2;
	pad[padding-1] = nh;

	plen = rlen + padding;
	mi = mo = m;
	ilen = olen = m->m_len - sizeof (struct ip);
	idat = odat = mtod(m, u_char *) + sizeof (struct ip);
	i = 0;

	/*
	 * We are now ready to encrypt the payload. 
	 */

	xd->edx_ivl++;
	
	iv[0] = xd->edx_iv[0];
	iv[1] = xd->edx_iv[1];
	iv[2] = xd->edx_iv[2];
	iv[3] = xd->edx_iv[3];
	if (xd->edx_ivlen == 4)
	{
		iv[4] = ~xd->edx_iv[0];
		iv[5] = ~xd->edx_iv[1];
		iv[6] = ~xd->edx_iv[2];
		iv[7] = ~xd->edx_iv[3];
	}
	else
	{
		iv[4] = xd->edx_iv[4];
		iv[5] = xd->edx_iv[5];
		iv[6] = xd->edx_iv[6];
		iv[7] = xd->edx_iv[7];
	}

	while (plen > 0)		/* while not done */
	{
		while (ilen == 0)	/* we exhausted previous mbuf */
		{
			mi = mi->m_next;
			if (mi == NULL)
			  panic("espdes_output: bad chain (i)\n");
			ilen = mi->m_len;
			idat = (u_char *)mi->m_data;
		}

		blk[i] = *idat++ ^ iv[i];
		
		i++;
		ilen--;

		if (i == 8)
		{
			des_ecb_encrypt(blk, blk, (caddr_t)(xd->edx_eks), 1);
			for (i=0; i<8; i++)
			{
				while (olen == 0)
				{
					mo = mo->m_next;
					if (mo == NULL)
					  panic("espdes_output: bad chain (o)\n");
					olen = mo->m_len;
					odat = (u_char *)mo->m_data;
				}
				*odat++ = blk[i];
				iv[i] = blk[i];
				olen--;
			}
			i = 0;
		}

		plen--;
	}

	/*
	 * Done with encryption. Let's wedge in the ESP header
	 * and send it out.
	 */

	M_PREPEND(m, ohlen, M_DONTWAIT);
	if (m == NULL)
	  return ENOBUFS;

	m = m_pullup(m, sizeof(struct ip) + xd->edx_ivlen + sizeof(u_long));
	if (m == NULL)
	  return ENOBUFS;

	ipo.ip_len = htons(sizeof (struct ip) + ohlen + rlen + padding);
	ipo.ip_p = IPPROTO_ESP;

	iv[0] = xd->edx_iv[0];
	iv[1] = xd->edx_iv[1];
	iv[2] = xd->edx_iv[2];
	iv[3] = xd->edx_iv[3];
	if (xd->edx_ivlen == 8)
	{
		iv[4] = xd->edx_iv[4];
		iv[5] = xd->edx_iv[5];
		iv[6] = xd->edx_iv[6];
		iv[7] = xd->edx_iv[7];
	}

	bcopy((caddr_t)&ipo, mtod(m, caddr_t), sizeof (struct ip));
	bcopy((caddr_t)&spi, mtod(m, caddr_t) + sizeof (struct ip), sizeof (u_long));
	bcopy((caddr_t)iv,  mtod(m, caddr_t) + sizeof (struct ip) + sizeof (u_long), xd->edx_ivlen);
	
	*mp = m;
	return 0;
}	
	


/*
 *
 *
 * m_pad(m, n) pads <m> with <n> bytes at the end. The packet header
 * length is updated, and a pointer to the first byte of the padding
 * (which is guaranteed to be all in one mbuf) is returned.
 *
 */

caddr_t
m_pad(struct mbuf *m, int n)
{
	register int len, pad;
	register struct mbuf *m0, *m1;
	caddr_t retval;
	u_char dat;
	
	if (n <= 0)			/* no stupid arguments */
	  return NULL;

	len = m->m_pkthdr.len;
	pad = n;

	m0 = m;

	while (m0->m_len < len)
	{
		len -= m0->m_len;
		m0 = m0->m_next;
	}

	if (m0->m_len != len)
	{
#ifdef ENCDEBUG
		if (encdebug)
		  printf("m_pad: length mismatch (should be %d instead of %d\n",
			 m->m_pkthdr.len, m->m_pkthdr.len + m0->m_len - len);
#endif ENCDEBUG
		m_freem(m);
		return NULL;
	}

	if ((m0->m_flags & M_EXT) ||
	    (m0->m_data + m0->m_len + pad >= &(m0->m_dat[MLEN])))
	{
		/*
		 * Add an mbuf to the chain
		 */

		MGET(m1, M_DONTWAIT, MT_DATA);
		if (m1 == 0)
		{
			m_freem(m0);
#ifdef ENCDEBUG
			if (encdebug)
			  printf("m_pad: cannot append\n");
#endif ENCDEBUG
			return NULL;
		}
		m0->m_next = m1;
		m0 = m1;
		m0->m_len = 0;
	}

	retval = m0->m_data + m0->m_len;
	m0->m_len += pad;
	m->m_pkthdr.len += pad;

	for (len = 0; len < n; len++)
	{
		get_random_bytes((void *)&dat, sizeof(dat));
	  	retval[len] = len + dat;
	}

	return retval;
}
