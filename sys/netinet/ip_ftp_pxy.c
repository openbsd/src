/*	$OpenBSD: ip_ftp_pxy.c,v 1.9 2000/04/05 05:35:27 kjell Exp $	*/

/*
 * Simple FTP transparent proxy for in-kernel use.  For use with the NAT
 * code.
 */
#if SOLARIS && defined(_KERNEL)
extern	kmutex_t	ipf_rw;
#endif

#define	isdigit(x)	((x) >= '0' && (x) <= '9')
#define	isupper(x)	((unsigned)((x) - 'A') <= 'Z' - 'A')

#define	IPF_FTP_PROXY

#define	IPF_MINPORTLEN	18
#define	IPF_MAXPORTLEN	30
#define	IPF_MIN227LEN	39
#define	IPF_MAX227LEN	51
#define	IPF_FTPBUFSZ	MAX(68,IPF_MAX227LEN)	/* This *MUST* be >= 51! */
			/* 68 is chosen as the minimum datagram size for */
			/* an unfragmented packet */


int ippr_ftp_init __P((void));
int ippr_ftp_new __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int ippr_ftp_out __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int ippr_ftp_in __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int ippr_ftp_portmsg __P((fr_info_t *, ip_t *, nat_t *));
int ippr_ftp_pasvmsg __P((fr_info_t *, ip_t *, nat_t *));
int ippr_ftp_complete __P((char *, size_t));

u_short ipf_ftp_atoi __P((char **));

static	frentry_t	natfr;
int	ippr_ftp_pasvonly = 0;


/*
 * Initialize local structures.
 */
int ippr_ftp_init()
{
	bzero((char *)&natfr, sizeof(natfr));
	natfr.fr_ref = 1;
	natfr.fr_flags = FR_INQUE|FR_PASS|FR_QUICK|FR_KEEPSTATE;
	return 0;
}


int ippr_ftp_complete(buf, len)
char *buf;
size_t len;
{
	register char *s, c;
	register size_t i;

	if (len < 5)
		return -1;
	s = buf;
	c = *s++;
	i = len - 1;

	if (isdigit(c)) {
		c = *s++;
		i--;
		if (isdigit(c)) {
			c = *s++;
			i--;
			if (isdigit(c)) {
				c = *s++;
				i--;
				if (c != '-' && c != ' ')
					return -1;
			} else
				return -1;
		} else
			return -1;
	} else if (isupper(c)) {
		c = *s++;
		i--;
		if (isupper(c)) {
			c = *s++;
			i--;
			if (isupper(c)) {
				c = *s++;
				i--;
				if (isupper(c)) {
					c = *s++;
					i--;
					if (c != ' ')
						return -1;
				} else
					return -1;
			} else
				return -1;
		} else
			return -1;
	} else
		return -1;

	for (; i && (c = *s); i--, s++) {
		if ((c == '\r') && (i != 2))
			return -1;
		if ((c == '\n') && (i != 1))
			return -1;
		else if ((i == 2) && (c != '\r'))
			return -1;
		else if ((i == 1) && (c != '\n'))
			return -1;
	}
	return i;
}


int ippr_ftp_new(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	ftpinfo_t *ftp;

	KMALLOC(ftp, ftpinfo_t *);
	if (ftp == NULL)
		return -1;
	aps->aps_data = ftp;
	aps->aps_psiz = sizeof(ftpinfo_t);

	ftp->ftp_passok = 0;
	return 0;
}


/*
 * ipf_ftp_atoi - implement a version of atoi which processes numbers in
 * pairs separated by commas (which are expected to be in the range 0 - 255),
 * returning a 16 bit number combining either side of the , as the MSB and
 * LSB.
 */
u_short ipf_ftp_atoi(ptr)
char **ptr;
{
	register char *s = *ptr, c;
	register u_char i = 0, j = 0;

	while ((c = *s++) && isdigit(c)) {
		i *= 10;
		i += c - '0';
	}
	if (c != ',') {
		*ptr = NULL;
		return 0;
	}
	while ((c = *s++) && isdigit(c)) {
		j *= 10;
		j += c - '0';
	}
	*ptr = s;
	return (i << 8) | j;
}


int ippr_ftp_portmsg(fin, ip, nat)
fr_info_t *fin;
ip_t *ip;
nat_t *nat;
{
	char portbuf[IPF_FTPBUFSZ], newbuf[IPF_FTPBUFSZ], *s;
	tcphdr_t *tcp, tcph, *tcp2 = &tcph;
	size_t nlen = 0, dlen, olen;
	u_short a5, a6, sp, dp;
	u_int a1, a2, a3, a4;
	struct in_addr swip;
	int off, inc = 0;
	ftpinfo_t *ftp;
	fr_info_t fi;
	nat_t *ipn;
	mb_t *m;
#if	SOLARIS
	mb_t *m1;
#endif

	tcp = (tcphdr_t *)fin->fin_dp;
	bzero(portbuf, sizeof(portbuf));
	off = (ip->ip_hl << 2) + (tcp->th_off << 2);

#if	SOLARIS
	m = fin->fin_qfm;

	dlen = msgdsize(m) - off;
	if (dlen > 0)
		copyout_mblk(m, off, MIN(sizeof(portbuf), dlen), portbuf);
#else
	m = *(mb_t **)fin->fin_mp;

	dlen = mbufchainlen(m) - off;
	if (dlen > 0)
		m_copydata(m, off, MIN(sizeof(portbuf), dlen), portbuf);
#endif
	if (dlen == 0)
		return 0;
	portbuf[sizeof(portbuf) - 1] = '\0';
	*newbuf = '\0';

	/*
	 * Check that a user is progressing through the login ok.
	 */
	if (ippr_ftp_complete(portbuf, dlen))
		return 0;
	ftp = nat->nat_aps->aps_data;
	switch (ftp->ftp_passok)
	{
	case 0 :
		if (!strncmp(portbuf, "USER ", 5))
			ftp->ftp_passok = 1;
		break;
	case 2 :
		if (!strncmp(portbuf, "PASS ", 5))
			ftp->ftp_passok = 3;
		break;
	}
	if (ftp->ftp_passok != 4)
		return 0;
	/*
	 * Check for client sending out PORT message.
	 */
	if (!ippr_ftp_pasvonly && !strncmp(portbuf, "PORT ", 5)) { 
		if (dlen < IPF_MINPORTLEN)
			return 0;
	} else
		return 0;

	/*
	 * Skip the PORT command + space
	 */
	s = portbuf + 5;
	/*
	 * Pick out the address components, two at a time.
	 */
	a1 = ipf_ftp_atoi(&s);
	if (!s)
		return 0;
	a2 = ipf_ftp_atoi(&s);
	if (!s)
		return 0;

	/*
	 * check that IP address in the PORT/PASV reply is the same as the
	 * sender of the command - prevents using PORT for port scanning.
	 */
	a1 <<= 16;
	a1 |= a2;
	if (a1 != ntohl(nat->nat_inip.s_addr))
		return 0;

	a5 = ipf_ftp_atoi(&s);
	if (!s)
		return 0;
	if (*s == ')')
		s++;

	/*
	 * check for CR-LF at the end.
	 */
	if (*s == '\n')
		s--;
	if ((*s == '\r') && (*(s + 1) == '\n')) {
		s += 2;
		a6 = a5 & 0xff;
	} else
		return 0;
	a5 >>= 8;
	/*
	 * Calculate new address parts for PORT command
	 */
	a1 = ntohl(ip->ip_src.s_addr);
	a2 = (a1 >> 16) & 0xff;
	a3 = (a1 >> 8) & 0xff;
	a4 = a1 & 0xff;
	a1 >>= 24;
	olen = s - portbuf;
	/* DO NOT change this to sprintf! */
	(void) sprintf(newbuf, "%s %u,%u,%u,%u,%u,%u\r\n",
		       "PORT", a1, a2, a3, a4, a5, a6);

	nlen = strlen(newbuf);
	inc = nlen - olen;
#if SOLARIS
	for (m1 = m; m1->b_cont; m1 = m1->b_cont)
		;
	if ((inc > 0) && (m1->b_datap->db_lim - m1->b_wptr < inc)) {
		mblk_t *nm;

		/* alloc enough to keep same trailer space for lower driver */
		nm = allocb(nlen, BPRI_MED);
		PANIC((!nm),("ippr_ftp_out: allocb failed"));

		nm->b_band = m1->b_band;
		nm->b_wptr += nlen;

		m1->b_wptr -= olen;
		PANIC((m1->b_wptr < m1->b_rptr),
		      ("ippr_ftp_out: cannot handle fragmented data block"));

		linkb(m1, nm);
	} else {
		if (m1->b_datap->db_struiolim == m1->b_wptr)
			m1->b_datap->db_struiolim += inc;
		m1->b_datap->db_struioflag &= ~STRUIO_IP;
		m1->b_wptr += inc;
	}
	copyin_mblk(m, off, nlen, newbuf);
#else
	if (inc < 0)
		m_adj(m, inc);
	/* the mbuf chain will be extended if necessary by m_copyback() */
	m_copyback(m, off, nlen, newbuf);
#endif
	if (inc != 0) {
#if SOLARIS || defined(__sgi)
		register u_32_t	sum1, sum2;

		sum1 = ip->ip_len;
		sum2 = ip->ip_len + inc;

		/* Because ~1 == -2, We really need ~1 == -1 */
		if (sum1 > sum2)
			sum2--;
		sum2 -= sum1;
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);

		fix_outcksum(&ip->ip_sum, sum2, 0);
#endif
		ip->ip_len += inc;
	}

	/*
	 * Add skeleton NAT entry for connection which will come back the
	 * other way.
	 */
	sp = htons(a5 << 8 | a6);
	/*
	 * Don't allow the PORT command to specify a port < 1024 due to
	 * security crap.
	 */
	if (ntohs(sp) < 1024)
		return 0;
	/*
	 * The server may not make the connection back from port 20, but
	 * it is the most likely so use it here to check for a conflicting
	 * mapping.
	 */
	dp = htons(fin->fin_data[1] - 1);
	ipn = nat_outlookup(fin->fin_ifp, IPN_TCP, nat->nat_p, nat->nat_inip,
			    ip->ip_dst, (dp << 16) | sp);
	if (ipn == NULL) {
		bcopy((char *)fin, (char *)&fi, sizeof(fi));
		bzero((char *)tcp2, sizeof(*tcp2));
		tcp2->th_win = htons(8192);
		tcp2->th_sport = sp;
		tcp2->th_dport = 0; /* XXX - don't specify remote port */
		fi.fin_data[0] = ntohs(sp);
		fi.fin_data[1] = 0;
		fi.fin_dp = (char *)tcp2;
		swip = ip->ip_src;
		ip->ip_src = nat->nat_inip;
		ipn = nat_new(nat->nat_ptr, ip, &fi, IPN_TCP|FI_W_DPORT,
			      NAT_OUTBOUND);
		if (ipn != NULL) {
			ipn->nat_age = fr_defnatage;
			(void) fr_addstate(ip, &fi, FI_W_DPORT);
		}
		ip->ip_src = swip;
	}
	return inc;
}


int ippr_ftp_out(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	return ippr_ftp_portmsg(fin, ip, nat);
}


int ippr_ftp_pasvmsg(fin, ip, nat)
fr_info_t *fin;
ip_t *ip;
nat_t *nat;
{
	char portbuf[IPF_FTPBUFSZ], newbuf[IPF_FTPBUFSZ], *s;
	int off, olen, dlen, nlen = 0, inc = 0;
	tcphdr_t tcph, *tcp2 = &tcph;
	struct in_addr swip, swip2;
	u_short a5, a6, dp, sp;
	u_int a1, a2, a3, a4;
	ftpinfo_t *ftp;
	tcphdr_t *tcp;
	fr_info_t fi;
	nat_t *ipn;
	mb_t *m;
#if	SOLARIS
	mb_t *m1;
#endif

	tcp = (tcphdr_t *)fin->fin_dp;
	off = (ip->ip_hl << 2) + (tcp->th_off << 2);
	m = *(mb_t **)fin->fin_mp;
	bzero(portbuf, sizeof(portbuf));

#if	SOLARIS
	m = fin->fin_qfm;

	dlen = msgdsize(m) - off;
	if (dlen > 0)
		copyout_mblk(m, off, MIN(sizeof(portbuf), dlen), portbuf);
#else
	dlen = mbufchainlen(m) - off;
	if (dlen > 0)
		m_copydata(m, off, MIN(sizeof(portbuf), dlen), portbuf);
#endif
	if (dlen == 0)
		return 0;
	portbuf[sizeof(portbuf) - 1] = '\0';
	*newbuf = '\0';

	/*
	 * Check that a user is progressing through the login ok.
	 * Don't put the switch in one common function because one side
	 * should only see numeric responses and the other commands.
	 */
	if (ippr_ftp_complete(portbuf, dlen))
		return 0;
	ftp = nat->nat_aps->aps_data;
	switch (ftp->ftp_passok)
	{
	case 1 :
		if (!strncmp(portbuf, "331", 3))
			ftp->ftp_passok = 2;
		else if (!strncmp(portbuf, "520", 3))
			ftp->ftp_passok = 0;
		break;
	case 3 :
		if (!strncmp(portbuf, "230", 3))
			ftp->ftp_passok = 4;
		break;
	default :
		break;
	}

	if (ftp->ftp_passok != 4)
		return 0;
	/*
	 * Check for PASV reply message.
	 */
	if (!strncmp(portbuf, "227 ", 4)) {
		if (dlen < IPF_MIN227LEN)
			return 0;
		else if (strncmp(portbuf, "227 Entering Passive Mode", 25))
			return 0;
	} else
		return 0;
	/*
	 * Skip the PORT command + space
	 */
	s = portbuf + 25;
	while (*s && !isdigit(*s))
		s++;
	/*
	 * Pick out the address components, two at a time.
	 */
	a1 = ipf_ftp_atoi(&s);
	if (!s)
		return 0;
	a2 = ipf_ftp_atoi(&s);
	if (!s)
		return 0;

	/*
	 * check that IP address in the PORT/PASV reply is the same as the
	 * sender of the command - prevents using PORT for port scanning.
	 */
	a1 <<= 16;
	a1 |= a2;
	if (a1 != ntohl(nat->nat_oip.s_addr))
		return 0;

	a5 = ipf_ftp_atoi(&s);
	if (!s)
		return 0;

	if (*s == ')')
		s++;
	if (*s == '\n')
		s--;
	/*
	 * check for CR-LF at the end.
	 */
	if ((*s == '\r') && (*(s + 1) == '\n')) {
		s += 2;
		a6 = a5 & 0xff;
	} else
		return 0;
	a5 >>= 8;
	/*
	 * Calculate new address parts for 227 reply
	 */
	a1 = ntohl(ip->ip_src.s_addr);
	a2 = (a1 >> 16) & 0xff;
	a3 = (a1 >> 8) & 0xff;
	a4 = a1 & 0xff;
	a1 >>= 24;
	olen = s - portbuf;
	(void) sprintf(newbuf, "%s %u,%u,%u,%u,%u,%u\r\n",
		       "227 Entering Passive Mode", a1, a2, a3, a4, a5, a6);

	nlen = strlen(newbuf);
	inc = nlen - olen;
#if SOLARIS
	for (m1 = m; m1->b_cont; m1 = m1->b_cont)
		;
	if ((inc > 0) && (m1->b_datap->db_lim - m1->b_wptr < inc)) {
		mblk_t *nm;

		/* alloc enough to keep same trailer space for lower driver */
		nm = allocb(nlen, BPRI_MED);
		PANIC((!nm),("ippr_ftp_out: allocb failed"));

		nm->b_band = m1->b_band;
		nm->b_wptr += nlen;

		m1->b_wptr -= olen;
		PANIC((m1->b_wptr < m1->b_rptr),
		      ("ippr_ftp_out: cannot handle fragmented data block"));

		linkb(m1, nm);
	} else {
		m1->b_wptr += inc;
	}
	copyin_mblk(m, off, nlen, newbuf);
#else
	if (inc < 0)
		m_adj(m, inc);
	/* the mbuf chain will be extended if necessary by m_copyback() */
	m_copyback(m, off, nlen, newbuf);
#endif
	if (inc != 0) {
#if SOLARIS || defined(__sgi)
		register u_32_t	sum1, sum2;

		sum1 = ip->ip_len;
		sum2 = ip->ip_len + inc;

		/* Because ~1 == -2, We really need ~1 == -1 */
		if (sum1 > sum2)
			sum2--;
		sum2 -= sum1;
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);

		fix_outcksum(&ip->ip_sum, sum2, 0);
#endif
		ip->ip_len += inc;
	}

	/*
	 * Add skeleton NAT entry for connection which will come back the
	 * other way.
	 */
	sp = 0;
	dp = htons(fin->fin_data[1] - 1);
	ipn = nat_outlookup(fin->fin_ifp, IPN_TCP, nat->nat_p, nat->nat_inip,
			    ip->ip_dst, (dp << 16) | sp);
	if (ipn == NULL) {
		bcopy((char *)fin, (char *)&fi, sizeof(fi));
		bzero((char *)tcp2, sizeof(*tcp2));
		tcp2->th_win = htons(8192);
		tcp2->th_sport = 0;		/* XXX - fake it for nat_new */
		fi.fin_data[0] = a5 << 8 | a6;
		tcp2->th_dport = htons(fi.fin_data[0]);
		fi.fin_data[1] = 0;
		fi.fin_dp = (char *)tcp2;
		swip = ip->ip_src;
		swip2 = ip->ip_dst;
		ip->ip_dst = ip->ip_src;
		ip->ip_src = nat->nat_inip;
		ipn = nat_new(nat->nat_ptr, ip, &fi, IPN_TCP|FI_W_SPORT,
			      NAT_OUTBOUND);
		if (ipn != NULL) {
			ipn->nat_age = fr_defnatage;
			(void) fr_addstate(ip, &fi, FI_W_SPORT);
		}
		ip->ip_src = swip;
		ip->ip_dst = swip2;
	}
	return inc;
}


int ippr_ftp_in(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{

	return ippr_ftp_pasvmsg(fin, ip, nat);
}
