/*	$OpenBSD: ip_ftp_pxy.c,v 1.4 1999/02/05 05:58:52 deraadt Exp $
 * $Id: ip_ftp_pxy.c,v 1.4 1999/02/05 05:58:52 deraadt Exp $
 * Simple FTP transparent proxy for in-kernel use.  For use with the NAT
 * code.
 */
#if SOLARIS && defined(_KERNEL)
extern	kmutex_t	ipf_rw;
#endif

#define	isdigit(x)	((x) >= '0' && (x) <= '9')

#define	IPF_FTP_PROXY

#define	IPF_MINPORTLEN	18
#define	IPF_MAXPORTLEN	30
#define	IPF_MIN227LEN	39
#define	IPF_MAX227LEN	51


int ippr_ftp_init __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int ippr_ftp_out __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int ippr_ftp_in __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int ippr_ftp_portmsg __P((fr_info_t *, ip_t *, nat_t *nat));
int ippr_ftp_pasvmsg __P((fr_info_t *, ip_t *, tcphdr_t *, nat_t *));
u_short ipf_ftp_atoi __P((char **));


/*
 * FTP application proxy initialization.
 */
int ippr_ftp_init(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	tcphdr_t *tcp = (tcphdr_t *)fin->fin_dp;

	aps->aps_sport = tcp->th_sport;
	aps->aps_dport = tcp->th_dport;
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
	char portbuf[IPF_MAXPORTLEN + 1], newbuf[IPF_MAXPORTLEN + 1], *s;
	int off, olen, dlen, nlen = 0, inc = 0;
	u_int a1, a2, a3, a4;
	tcphdr_t *tcp, tcph, *tcp2 = &tcph;
	struct in_addr swip;
	u_short a5, a6, sp, dp;
	fr_info_t fi;
	nat_t *ipn;
	mb_t *m;
#if	SOLARIS
	mb_t *m1;
#endif

	tcp = (tcphdr_t *)fin->fin_dp;
	off = (ip->ip_hl << 2) + (tcp->th_off << 2);
	m = *(mb_t **)fin->fin_mp;

#if	SOLARIS
	m = fin->fin_qfm;

	dlen = msgdsize(m) - off;
	bzero(portbuf, sizeof(portbuf));
	copyout_mblk(m, off, MIN(sizeof(portbuf), dlen), portbuf);
#else
	dlen = mbufchainlen(m) - off;
	bzero(portbuf, sizeof(portbuf));
	m_copydata(m, off, MIN(sizeof(portbuf), dlen), portbuf);
#endif
	portbuf[sizeof(portbuf) - 1] = '\0';
	*newbuf = '\0';

	if (!strncmp(portbuf, "PORT ", 5)) { 
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

	/*
	 * check for CR-LF at the end.
	 */
	if (((*s == '\r') && (*(s + 1) == '\n')) ||
	    ((*(s - 1) == '\r') && (*s == '\n')))
		a6 = a5 & 0xff;
	else
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
	olen = s - portbuf + 1;
	(void) sprintf(newbuf, "%s %d,%d,%d,%d,%d,%d\r\n",
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
		m1->b_wptr += inc;
	}
	copyin_mblk(m, off, nlen, newbuf);
#else
	if (inc < 0)
		m_adj(m, inc);
	/* the mbuf chain will be extended if necessary by m_copyback() */
	m_copyback(m, off, nlen, newbuf);
#endif
	if (inc) {
#if SOLARIS || defined(__sgi)
		register u_32_t	sum1, sum2;

		sum1 = ip->ip_len;
		sum2 = ip->ip_len + inc;

		/* Because ~1 == -2, We really need ~1 == -1 */
		if (sum1 > sum2)
			sum2--;
		sum2 -= sum1;
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);

		fix_outcksum(&ip->ip_sum, sum2);
#endif
		ip->ip_len += inc;
	}

	/*
	* Add skeleton NAT entry for connection which will come back the
	* other way.
	*/
	sp = htons(a5 << 8 | a6);
	dp = htons(fin->fin_data[1] - 1);
	ipn = nat_outlookup(fin->fin_ifp, IPN_TCP, nat->nat_inip, sp,
			    ip->ip_dst, dp);
	if (ipn == NULL) {
		bcopy((char *)fin, (char *)&fi, sizeof(fi));
		bzero((char *)tcp2, sizeof(*tcp2));
		tcp2->th_sport = sp;
		tcp2->th_dport = dp;
		fi.fin_dp = (char *)tcp2;
		swip = ip->ip_src;
		ip->ip_src = nat->nat_inip;
		ipn = nat_new(nat->nat_ptr, ip, &fi, IPN_TCP, NAT_OUTBOUND);
		if (ipn != NULL) {
			ipn->nat_age = fr_defnatage;
			(void) fr_addstate(ip, &fi, FR_INQUE|FR_PASS|
						    FR_QUICK|FR_KEEPSTATE);
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


int ippr_ftp_pasvmsg(fin, ip, tcp, nat)
fr_info_t *fin;
ip_t *ip;
tcphdr_t *tcp;
nat_t *nat;
{
	char portbuf[IPF_MAX227LEN + 1], newbuf[IPF_MAX227LEN + 1], *s;
	int off, olen, dlen, nlen = 0, inc = 0;
	u_int a1, a2, a3, a4;
	tcphdr_t tcph, *tcp2 = &tcph;
	struct in_addr swip;
	u_short a5, a6;
	fr_info_t fi;
	nat_t *ipn;
	mb_t *m;
#if	SOLARIS
	mb_t *m1;
#endif

	off = (ip->ip_hl << 2) + (tcp->th_off << 2);
	m = *(mb_t **)fin->fin_mp;

#if	SOLARIS
	m = fin->fin_qfm;

	dlen = msgdsize(m) - off;
	bzero(portbuf, sizeof(portbuf));
	copyout_mblk(m, off, MIN(sizeof(portbuf), dlen), portbuf);
#else
	dlen = mbufchainlen(m) - off;
	bzero(portbuf, sizeof(portbuf));
	m_copydata(m, off, MIN(sizeof(portbuf), dlen), portbuf);
#endif
	portbuf[sizeof(portbuf) - 1] = '\0';
	*newbuf = '\0';

	if (!strncmp(portbuf, "227 ", 4)) {
		if (dlen < IPF_MIN227LEN)
			return 0;
		else if (strncmp(portbuf, "227 Entering Passive Mode", 25))
			return 0;
#ifndef notyet
		return 0;
#endif
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

	/*
	* check for CR-LF at the end.
	*/
	if (((*s == '\r') && (*(s + 1) == '\n')) ||
	    ((*(s - 1) == '\r') && (*s == '\n')))
		a6 = a5 & 0xff;
	else
		return 0;
	a5 >>= 8;
	/*
	* Calculate new address parts for 227 reply
	*/
	a1 = ntohl(nat->nat_inip.s_addr);
	a2 = (a1 >> 16) & 0xff;
	a3 = (a1 >> 8) & 0xff;
	a4 = a1 & 0xff;
	a1 >>= 24;
	olen = s - portbuf + 1;
	(void) sprintf(newbuf, "%s %d,%d,%d,%d,%d,%d\r\n",
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
	if (inc) {
#if SOLARIS || defined(__sgi)
		register u_32_t	sum1, sum2;

		sum1 = ip->ip_len;
		sum2 = ip->ip_len + inc;

		/* Because ~1 == -2, We really need ~1 == -1 */
		if (sum1 > sum2)
			sum2--;
		sum2 -= sum1;
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);

		fix_outcksum(&ip->ip_sum, sum2);
#endif
		ip->ip_len += inc;
	}

	/*
	 * Add skeleton NAT entry for connection which will come back the
	 * other way.
	 */
	bcopy((char *)fin, (char *)&fi, sizeof(fi));
	bzero((char *)tcp2, sizeof(*tcp2));
	tcp2->th_sport = htons(a5 << 8 | a6);
	fi.fin_dp = (char *)tcp2;
	swip = ip->ip_src;
	ip->ip_src = nat->nat_oip;
	ipn = nat_new(nat->nat_ptr, ip, &fi, IPN_TCP, NAT_INBOUND);
	if (ipn != NULL)
		ipn->nat_age = fr_defnatage;
	(void) fr_addstate(ip, &fi, FR_INQUE|FR_PASS|FR_QUICK|FR_KEEPSTATE);
	ip->ip_src = swip;
	return inc;
}

int ippr_ftp_in(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	tcphdr_t *tcp = (tcphdr_t *)fin->fin_dp;

	return ippr_ftp_pasvmsg(fin, ip, tcp, nat);
}
