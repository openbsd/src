/*	$OpenBSD: ipnat.c,v 1.2 1996/06/23 14:31:01 deraadt Exp $	*/

/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * Added redirect stuff and a variety of bug fixes. (mcn@EnGarde.com)
 *
 * Broken still:
 * Displaying the nat with redirect entries is way confusing
 *
 * Example redirection line:
 * rdr le1 0.0.0.0/0 port 79 -> 199.165.219.129 port 9901
 * 
 * Will redirect all incoming packets on le1 to any machine, port 79 to
 * host 199.165.219.129, port 9901
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#if defined(sun) && (defined(__svr4__) || defined(__SVR4))
# include <sys/ioccom.h>
# include <sys/sysmacros.h>
#endif
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include "ip_fil.h"
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include "ip_nat.h"
#include <ctype.h>


#ifndef lint
static  char    sccsid[] ="@(#)ipnat.c	1.8 4/10/96 (C) 1993 Darren Reed";
#endif

#if	SOLARIS
#define	bzero(a,b)	memset(a,0,b)
#endif

extern	char	*optarg;
extern	int	kmemcpy();

void	dostats(), printnat(), parsefile();

int main(argc, argv)
int argc;
char *argv[];
{
	char	*file = NULL, c;
	int	fd, opts = 1;

	while ((c = getopt(argc, argv, "f:lnrsv")) != -1)
		switch (c)
		{
		case 'f' :
			file = optarg;
			break;
		case 'l' :
			opts |= 8;
			break;
		case 'n' :
			opts |= 2;
			break;
		case 'r' :
			opts &= ~1;
			break;
		case 's' :
			opts |= 4;
			break;
		case 'v' :
			opts |= 16;
			break;
		default :
			fprintf(stderr, "unknown option \"%c\"\n", c);
			break;
		}

	if ((fd = open(IPL_NAME, O_RDONLY)) == -1) {
		perror("open");
		exit(-1);
	}

	if (file)
		parsefile(fd, file, opts);
	if (opts & 12)
		dostats(fd);
	return 0;
}


void printnat(np)
ipnat_t *np;
{
	if (np->in_redir == NAT_REDIRECT) {
		printf("rdr %s %s", np->in_ifname, inet_ntoa(np->in_out[0]));
		printf("/%s (%d) -> ", inet_ntoa(np->in_out[1]),
		       ntohs(np->in_pmin));
		printf("%s (%d)\n", inet_ntoa(np->in_in[0]),
		       ntohs(np->in_pnext));
		printf("\t%x %u %x %u\n", (u_int)np->in_ifp, np->in_space,
		       np->in_flags, np->in_pnext);
	} else {
		np->in_nextip.s_addr = htonl(np->in_nextip.s_addr);
		printf("map %s %s/", np->in_ifname, inet_ntoa(np->in_in[0]));
		printf("%s -> ", inet_ntoa(np->in_in[1]));
		printf("%s/", inet_ntoa(np->in_out[0]));
		printf("%s\n", inet_ntoa(np->in_out[1]));
		printf("\t%x %u %s %x %u %d:%d\n", (u_int)np->in_ifp,
		       np->in_space, inet_ntoa(np->in_nextip), np->in_flags,
		       np->in_pnext, ntohs(np->in_port[0]),
		       ntohs(np->in_port[1]));
	}
}


void dostats(fd, opts)
int fd, opts;
{
	natstat_t ns;
	ipnat_t	ipn;
	nat_t	**nt, *np, nat;
	int	i;

	if (ioctl(fd, SIOCGNATS, &ns) == -1) {
		perror("ioctl(SIOCGNATS)");
		return;
	}
	if (opts & 4) {
		printf("mapped\tin\t%lu\tout\t%lu\n",
			ns.ns_mapped[0], ns.ns_mapped[1]);
		printf("added\t%lu\texpired\t%lu\n",
			ns.ns_added, ns.ns_expire);
		printf("inuse\t%lu\n", ns.ns_inuse);
		printf("table %#x list %#x\n",
			(u_int)ns.ns_table, (u_int)ns.ns_list);
	}
	if (opts & 8) {
		while (ns.ns_list) {
			if (kmemcpy(&ipn, ns.ns_list, sizeof(ipn))) {
				perror("kmemcpy");
				break;
			}
			printnat(&ipn);
			ns.ns_list = ipn.in_next;
		}

		nt = (nat_t **)malloc(sizeof(*nt) * NAT_SIZE);
		if (kmemcpy(nt, ns.ns_table, sizeof(*nt) * NAT_SIZE)) {
			perror("kmemcpy");
			return;
		}
		for (i = 0; i < NAT_SIZE; i++)
			for (np = nt[i]; np; np = nat.nat_next) {
				if (kmemcpy(&nat, np, sizeof(nat)))
					break;
				printf("%s %hu <- -> ",
					inet_ntoa(nat.nat_inip),
					ntohs(nat.nat_inport));
				printf("%s %hu %hu %hu %lx [",
					inet_ntoa(nat.nat_outip),
					ntohs(nat.nat_outport),
					nat.nat_age, nat.nat_use,
					nat.nat_sumd);
				printf("%s %hu]\n", inet_ntoa(nat.nat_oip),
					ntohs(nat.nat_oport));
			}
	}
}


u_short	portnum(name, proto)
char	*name, *proto;
{
	struct	servent *sp, *sp2;
	u_short	p1 = 0;

	if (isdigit(*name))
		return htons((u_short)atoi(name));
	if (!proto)
		proto = "tcp/udp";
	if (strcasecmp(proto, "tcp/udp")) {
		sp = getservbyname(name, proto);
		if (sp)
			return sp->s_port;
		(void) fprintf(stderr, "unknown service \"%s\".\n", name);
		return 0;
	}
	sp = getservbyname(name, "tcp");
	if (sp)
		p1 = sp->s_port;
	sp2 = getservbyname(name, "udp");
	if (!sp || !sp2) {
		(void) fprintf(stderr, "unknown tcp/udp service \"%s\".\n",
			name);
		return 0;
	}
	if (p1 != sp2->s_port) {
		(void) fprintf(stderr, "%s %d/tcp is a different port to ",
			name, p1);
		(void) fprintf(stderr, "%s %d/udp\n", name, sp->s_port);
		return 0;
	}
	return p1;
}


u_long	hostmask(msk)
char	*msk;
{
	int	bits = -1;
	u_long	mask;

	if (!isdigit(*msk))
		return (u_long)-1;
	if (strchr(msk, '.'))
		return inet_addr(msk);
	if (strchr(msk, 'x'))
		return (u_long)strtol(msk, NULL, 0);
	/*
	 * set x most significant bits
	 */
	for (mask = 0, bits = atoi(msk); bits; bits--) {
		mask /= 2;
		mask |= ntohl(inet_addr("128.0.0.0"));
	}
	mask = htonl(mask);
	return mask;
}


/*
 * returns an ip address as a long var as a result of either a DNS lookup or
 * straight inet_addr() call
 */
u_long	hostnum(host, resolved)
char	*host;
int	*resolved;
{
	struct	hostent	*hp;
	struct	netent	*np;

	*resolved = 0;
	if (!strcasecmp("any",host))
		return 0L;
	if (isdigit(*host))
		return inet_addr(host);

	if (!(hp = gethostbyname(host))) {
		if (!(np = getnetbyname(host))) {
			*resolved = -1;
			fprintf(stderr, "can't resolve hostname: %s\n", host);
			return 0;
		}
		return np->n_net;
	}
	return *(u_long *)hp->h_addr;
}


ipnat_t *parse(line)
char *line;
{
	static ipnat_t ipn;
	char *s, *t;
	char *shost, *snetm, *dhost, *dnetm, *proto, *dport, *tport;
	int resolved;

	bzero((char *)&ipn, sizeof(ipn));
	if ((s = strchr(line, '\n')))
		*s = '\0';
	if ((s = strchr(line, '#')))
		*s = '\0';
	if (!*line)
		return NULL;
	if (!(s = strtok(line, " \t")))
		return NULL;
	if (!strcasecmp(s, "map"))
		ipn.in_redir = NAT_MAP;
	else if (!strcasecmp(s, "rdr"))
		ipn.in_redir = NAT_REDIRECT;
	else {
		(void)fprintf(stderr,
			      "expected \"map\" or \"rdr\", got \"%s\"\n", s);
		return NULL;
	}

	if (!(s = strtok(NULL, " \t"))) {
		fprintf(stderr, "missing fields (interface)\n");
		return NULL;
	}
	strncpy(ipn.in_ifname, s, sizeof(ipn.in_ifname) - 1);
	ipn.in_ifname[sizeof(ipn.in_ifname) - 1] = '\0';
	if (!(s = strtok(NULL, " \t"))) {
		fprintf(stderr, "missing fields (%s)\n", 
			ipn.in_redir ? "destination": "source");
		return NULL;
	}
	shost = s;

	if (ipn.in_redir == NAT_REDIRECT) {
		if (!(s = strtok(NULL, " \t"))) {
			fprintf(stderr, "missing fields (destination port)\n");
			return NULL;
		}

		if (strcasecmp(s, "port")) {
			fprintf(stderr, "missing fields (port)\n");
			return NULL;
		}

		if (!(s = strtok(NULL, " \t"))) {
			fprintf(stderr, "missing fields (destination port)\n");
			return NULL;
		}

		dport = s;
	}


	if (!(s = strtok(NULL, " \t"))) {
		fprintf(stderr, "missing fields (->)\n");
		return NULL;
	}
	if (!strcmp(s, "->")) {
		snetm = strrchr(shost, '/');
		if (!snetm) {
			fprintf(stderr, "missing fields (%s netmask)\n",
				ipn.in_redir ? "destination":"source");
			return NULL;
		}
	} else {
		if (strcasecmp(s, "netmask")) {
			fprintf(stderr, "missing fields (netmask)\n");
			return NULL;
		}
		if (!(s = strtok(NULL, " \t"))) {
			fprintf(stderr, "missing fields (%s netmask)\n",
				ipn.in_redir ? "destination":"source");
			return NULL;
		}
		snetm = s;
	}

        if (!(s = strtok(NULL, " \t"))) {
		fprintf(stderr, "missing fields (%s)\n",
			ipn.in_redir ? "destination":"target");
		return NULL;
	}
        dhost = s;

        if (ipn.in_redir == NAT_MAP) {
		if (!(s = strtok(NULL, " \t"))) {
			dnetm = strrchr(dhost, '/');
			if (!dnetm) {
				fprintf(stderr,
					"missing fields (dest netmask)\n");
				return NULL;
			}
		}
		if (!s || !strcasecmp(s, "portmap")) {
			dnetm = strrchr(dhost, '/');
			if (!dnetm) {
				fprintf(stderr,
					"missing fields (dest netmask)\n");
				return NULL;
			}
		} else {
			if (strcasecmp(s, "netmask")) {
				fprintf(stderr,
					"missing fields (dest netmask)\n");
				return NULL;
			}
			if (!(s = strtok(NULL, " \t"))) {
				fprintf(stderr,
					"missing fields (dest netmask)\n");
				return NULL;
			}
			dnetm = s;
		}
		if (*dnetm == '/')
			*dnetm++ = '\0';
	} else {
		/* If it's a in_redir, expect target port */
		if (!(s = strtok(NULL, " \t"))) {
			fprintf(stderr, "missing fields (destination port)\n");
			return NULL;
		}

		if (strcasecmp(s, "port")) {
			fprintf(stderr, "missing fields (port)\n");
			return NULL;
		}
	  
		if (!(s = strtok(NULL, " \t"))) {
			fprintf(stderr, "missing fields (destination port)\n");
			return NULL;
		}
	  
		tport = s;
	} 
	    

	if (*snetm == '/')
		*snetm++ = '\0';

	if (ipn.in_redir == NAT_MAP) {
		ipn.in_inip = hostnum(shost, &resolved);
		ipn.in_inmsk = hostmask(snetm);
		ipn.in_outip = hostnum(dhost, &resolved);
		ipn.in_outmsk = hostmask(dnetm);
	} else {
		ipn.in_inip = hostnum(dhost, &resolved); /* Inside is target */
		ipn.in_inmsk = hostmask("255.255.255.255");
		ipn.in_outip = hostnum(shost, &resolved);
		ipn.in_outmsk = hostmask(snetm);
		if (!(s = strtok(NULL, " \t"))) {
			ipn.in_flags = IPN_TCP; /* XXX- TCP only by default */
			proto = "tcp";
		} else {
			if (!strcasecmp(s, "tcp"))
				ipn.in_flags = IPN_TCP;
			else if (!strcasecmp(s, "udp"))
				ipn.in_flags = IPN_UDP;
			else if (!strcasecmp(s, "tcpudp"))
				ipn.in_flags = IPN_TCPUDP;
			else {
				fprintf(stderr,
					"expected protocol - got \"%s\"\n", s);
				return NULL;
			}
			proto = s;
		}
		ipn.in_pmin = portnum(dport, proto); /* dest port */
		ipn.in_pmax = ipn.in_pmin; /* NECESSARY of removing nats */
		ipn.in_pnext = portnum(tport, proto); /* target port */
		s = NULL; /* That's all she wrote! */
	}
	if (!s)
		return &ipn;
	if (strcasecmp(s, "portmap")) {
		fprintf(stderr, "expected \"portmap\" - got \"%s\"\n", s);
		return NULL;
	}
	if (!(s = strtok(NULL, " \t")))
		return NULL;
	if (!strcasecmp(s, "tcp"))
		ipn.in_flags = IPN_TCP;
	else if (!strcasecmp(s, "udp"))
		ipn.in_flags = IPN_UDP;
	else if (!strcasecmp(s, "tcpudp"))
		ipn.in_flags = IPN_TCPUDP;
	else {
		fprintf(stderr, "expected protocol name - got \"%s\"\n", s);
		return NULL;
	}
	proto = s;
	if (!(s = strtok(NULL, " \t"))) {
		fprintf(stderr, "no port range found\n");
		return NULL;
	}
	if (!(t = strchr(s, ':'))) {
		fprintf(stderr, "no port range in \"%s\"\n", s);
		return NULL;
	}
	*t++ = '\0';
	ipn.in_pmin = portnum(s, proto);
	ipn.in_pmax = portnum(t, proto);
	return &ipn;
}


void parsefile(fd, file, opts)
int fd;
char *file;
int opts;
{
	char	line[512], *s;
	ipnat_t	*np;
	FILE	*fp;
	int	linenum = 1;

	if (strcmp(file, "-"))
		fp = fopen(file, "r");
	else
		fp = stdin;

	while (fgets(line, sizeof(line) - 1, fp)) {
		line[sizeof(line) - 1] = '\0';
		if ((s = strchr(line, '\n')))
			*s = '\0';
		if (!(np = parse(line))) {
			if (*line)
				fprintf(stderr, "%d: syntax error in \"%s\"\n",
					linenum, line);
		} else if (!(opts & 2)) {
			if ((opts &16) && np)
				printnat(np);
			if (opts & 1) {
				if (ioctl(fd, SIOCADNAT, np) == -1)
					perror("ioctl(SIOCADNAT)");
			} else if (ioctl(fd, SIOCRMNAT, np) == -1)
				perror("ioctl(SIOCRMNAT)");
		}
		linenum++;
	}
	fclose(stdin);
}
