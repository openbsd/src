/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/syslog.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/uio.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <sys/dir.h>
#include <sys/mbuf.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/user.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <arpa/inet.h>

#ifndef	lint
static	char	sccsid[] = "@(#)ipmon.c	1.16 1/12/96 (C)1995 Darren Reed";
#endif

#include "ip_fil.h"

struct	flags {
	int	value;
	char	flag;
};

struct	flags	tcpfl[] = {
	{ TH_ACK, 'A' },
	{ TH_RST, 'R' },
	{ TH_SYN, 'S' },
	{ TH_FIN, 'F' },
	{ TH_URG, 'U' },
	{ TH_PUSH,'P' },
	{ 0, '\0' }
};

static	char	line[2048];
static	void	printpacket();


char	*hostname(res, ip)
int	res;
struct	in_addr	ip;
{
	struct hostent *hp;

	if (!res)
		return inet_ntoa(ip);
	hp = gethostbyaddr((char *)&ip, sizeof(ip), AF_INET);
	if (!hp)
		return inet_ntoa(ip);
	return hp->h_name;
}


char	*portname(res, proto, port)
int	res;
char	*proto;
u_short	port;
{
	static	char	pname[8];
	struct	servent	*serv;

	(void) sprintf(pname, "%hu", htons(port));
	if (!res)
		return pname;
	serv = getservbyport((int)port, proto);
	if (!serv)
		return pname;
	return serv->s_name;
}


static	void	printpacket(log, ip, lp, opts)
FILE	*log;
struct	ip	*ip;
struct	ipl_ci	*lp;
int	opts;
{
	struct	protoent *pr;
	struct	tcphdr	*tp;
	struct	icmp	*ic;
	struct	ip	*ipc;
	struct	tm	*tm;
	char	c[3], pname[8], *t, *proto;
	u_short	hl, p;
	int	i, lvl, res;

	res = (opts & 2) ? 1 : 0;
	t = line;
	*t = '\0';
	hl = (ip->ip_hl << 2);
	p = (u_short)ip->ip_p;
	tm = localtime((time_t *)&lp->sec);
	if (!(opts & 1)) {
		(void) sprintf(t, "%2d/%02d/%4d ",
			tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
		t += strlen(t);
	}
	(void) sprintf(t, "%02d:%02d:%02d.%-.6ld %c%c%ld @%hd ",
		tm->tm_hour, tm->tm_min, tm->tm_sec, lp->usec,
		lp->ifname[0], lp->ifname[1], lp->unit, lp->rule);
	pr = getprotobynumber((int)p);
	if (!pr) {
		proto = pname;
		sprintf(proto, "%d", (u_int)p);
	} else
		proto = pr->p_name;

 	if (lp->flags & (FI_SHORT << 20)) {
		c[0] = 'S';
		lvl = LOG_ERR;
	} else if (lp->flags & FR_PASS) {
		if (lp->flags & FR_LOGP)
			c[0] = 'p';
		else
			c[0] = 'P';
		lvl = LOG_NOTICE;
	} else if (lp->flags & FR_BLOCK) {
		if (lp->flags & FR_LOGB)
			c[0] = 'b';
		else
			c[0] = 'B';
		lvl = LOG_WARNING;
	} else if (lp->flags & FF_LOGNOMATCH) {
		c[0] = 'n';
		lvl = LOG_NOTICE;
	} else {
		c[0] = 'L';
		lvl = LOG_INFO;
	}
	c[1] = ' ';
	c[2] = '\0';
	(void) strcat(line, c);
	t = line + strlen(line);
#if	SOLARIS
	ip->ip_off = ntohs(ip->ip_off);
	ip->ip_len = ntohs(ip->ip_len);
#endif

	if ((p == IPPROTO_TCP || p == IPPROTO_UDP) && !(ip->ip_off & 0x1fff)) {
		tp = (struct tcphdr *)((char *)ip + hl);
		if (!(lp->flags & (FI_SHORT << 16))) {
			(void) sprintf(t, "%s,%s -> ",
				hostname(res, ip->ip_src),
				portname(res, proto, tp->th_sport));
			t += strlen(t);
			(void) sprintf(t, "%s,%s PR %s len %hu %hu ",
				hostname(res, ip->ip_dst),
				portname(res, proto, tp->th_dport),
				proto, hl, ip->ip_len);
			t += strlen(t);

			if (p == IPPROTO_TCP) {
				*t++ = '-';
				for (i = 0; tcpfl[i].value; i++)
					if (tp->th_flags & tcpfl[i].value)
						*t++ = tcpfl[i].flag;
			}
			*t = '\0';
		} else {
			(void) sprintf(t, "%s -> ", hostname(res, ip->ip_src));
			t += strlen(t);
			(void) sprintf(t, "%s PR %s len %hu %hu",
				hostname(res, ip->ip_dst), proto,
				hl, ip->ip_len);
		}
	} else if (p == IPPROTO_ICMP) {
		ic = (struct icmp *)((char *)ip + hl);
		(void) sprintf(t, "%s -> ", hostname(res, ip->ip_src));
		t += strlen(t);
		(void) sprintf(t, "%s PR icmp len %hu (%hu) icmp %d/%d",
			hostname(res, ip->ip_dst), hl,
			ip->ip_len, ic->icmp_type, ic->icmp_code);
		if (ic->icmp_type == ICMP_UNREACH ||
		    ic->icmp_type == ICMP_SOURCEQUENCH ||
		    ic->icmp_type == ICMP_PARAMPROB ||
		    ic->icmp_type == ICMP_REDIRECT ||
		    ic->icmp_type == ICMP_TIMXCEED) {
			ipc = &ic->icmp_ip;
			tp = (struct tcphdr *)((char *)ipc + hl);

			p = (u_short)ipc->ip_p;
			pr = getprotobynumber((int)p);
			if (!pr) {
				proto = pname;
				(void) sprintf(proto, "%d", (int)p);
			} else
				proto = pr->p_name;

			t += strlen(t);
			(void) sprintf(t, " for %s,%s -",
				hostname(res, ipc->ip_src),
				portname(res, proto, tp->th_sport));
			t += strlen(t);
			(void) sprintf(t, " %s,%s PR %s len %hu %hu",
				hostname(res, ipc->ip_dst),
				portname(res, proto, tp->th_dport),
				proto, ipc->ip_hl << 2, ipc->ip_len);
		}
	} else {
		(void) sprintf(t, "%s -> ", hostname(res, ip->ip_src));
		t += strlen(t);
		(void) sprintf(t, "%s PR %s len %hu (%hu)",
			hostname(res, ip->ip_dst), proto, hl, ip->ip_len);
		t += strlen(t);
		if (ip->ip_off & 0x1fff)
			(void) sprintf(t, " frag %s%s%hu@%hu",
				ip->ip_off & IP_MF ? "+" : "",
				ip->ip_off & IP_DF ? "-" : "",
				ip->ip_len - hl, (ip->ip_off & 0x1fff) << 3);
	}
	t += strlen(t);
	*t++ = '\n';
	*t++ = '\0';
	if (opts & 1)
		syslog(lvl, "%s", line);
	else
		(void) fprintf(log, "%s", line);
	fflush(log);
}

main(argc, argv)
int argc;
char *argv[];
{
	FILE		*log;
	int		fd, flushed = 0, opts = 0;
	char		buf[512], c;
	struct ipl_ci 	iplci;
	extern	int	optind;

	if ((fd = open(IPL_NAME, O_RDONLY)) == -1) {
		(void) fprintf(stderr, "%s: ", IPL_NAME);
		perror("open");
		exit(-1);
	}

	while ((c = getopt(argc, argv, "Nfs")) != -1)
		switch (c)
		{
		case 'f' :
			if (ioctl(fd, SIOCIPFFB, &flushed) == 0) {
				printf("%d bytes flushed from log buffer\n",
					flushed);
				fflush(stdout);
			}
			break;
		case 'N' :
			opts |= 2;
			break;
		case 's' :
			openlog(argv[0], LOG_NDELAY|LOG_PID, LOGFAC);
			opts |= 1;
			break;
		}

	log = argv[optind] ? fopen(argv[1], "a") : stdout;
	setvbuf(log, NULL, _IONBF, 0);
	if (flushed)
		fprintf(log, "%d bytes flushed from log\n", flushed);

	while (1) {
		assert(read(fd, &iplci, sizeof(struct ipl_ci)) ==
			sizeof(struct ipl_ci));
		assert(iplci.hlen > 0 && iplci.hlen <= 92);
		assert((u_char)iplci.plen <= 128);
		assert(read(fd, buf, iplci.hlen + iplci.plen) ==
			(iplci.hlen + iplci.plen));
		printpacket(log, buf, &iplci, opts);
	}
	return 0;
}
