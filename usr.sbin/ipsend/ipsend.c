/*
 * ipsend.c (C) 1995 Darren Reed
 *
 * This was written to test what size TCP fragments would get through
 * various TCP/IP packet filters, as used in IP firewalls.  In certain
 * conditions, enough of the TCP header is missing for unpredictable
 * results unless the filter is aware that this can happen.
 *
 * The author provides this program as-is, with no gaurantee for its
 * suitability for any specific purpose.  The author takes no responsibility
 * for the misuse/abuse of this program and provides it for the sole purpose
 * of testing packet filter policies.  This file maybe distributed freely
 * providing it is not modified and that this notice remains in tact.
 *
 * This was written and tested (successfully) on SunOS 4.1.x.
 */
#ifndef	lint
static	char	sccsid[] = "%W% %G% (C)1995 Darren Reed";
#endif
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#ifndef	linux
#include <netinet/ip_var.h>
#include <netinet/tcpip.h>
#endif
#include "ip_compat.h"
#ifdef	linux
#include <linux/sockios.h>
#include "tcpip.h"
#endif
#include "ipt.h"

#if defined (__OpenBSD__) || defined (__NetBSD__)
/* XXX - ipftest already has a pcap.h file, so just declare this here */
char *pcap_lookupdev (char *);
#endif

extern	char	*optarg;
extern	int	optind;
extern	struct	ipread	snoop, pcap;

struct	ipread	*readers[] = { &snoop, &pcap, NULL };
char	options[68];
#ifdef	linux
char	default_device[] = "eth0";
#else
# ifdef	sun
char	default_device[] = "le0";
# else
#  ifdef	ultrix
char	default_device[] = "ln0";
#  else
#   ifdef	__bsdi__
char	default_device[] = "ef0";
#   else
char	default_device[] = "le0";
#   endif
#  endif
# endif
#endif


void	usage(prog)
char	*prog;
{
	fprintf(stderr, "Usage: %s [options] dest [flags]\n\
\toptions:\n\
\t\t-d device\tSend out on this device\n\
\t\t-f fragflags\tcan set IP_MF or IP_DF\n\
\t\t-g gateway\tIP gateway to use if non-local dest.\n\
\t\t-I code,type[,gw[,dst[,src]]]\tSet ICMP protocol\n\
\t\t-m mtu\t\tfake MTU to use when sending out\n\
\t\t-P protocol\tSet protocol by name\n\
\t\t-s src\t\tsource address for IP packet\n\
\t\t-T\t\tSet TCP protocol\n\
\t\t-t port\t\tdestination port\n\
\t\t-U\t\tSet UDP protocol\n\
", prog);
	fprintf(stderr, "Usage: %s [options] -g gateway\n\
\toptions:\n\
\t\t-r filename\tsnoop data file to resend\n\
\t\t-R filename\tlibpcap data file to resend\n\
", prog);
	fprintf(stderr, "Usage: %s [options] destination\n\
\toptions:\n\
\t\t-d device\tSend out on this device\n\
\t\t-m mtu\t\tfake MTU to use when sending out\n\
\t\t-s src\t\tsource address for IP packet\n\
\t\t-1 \t\tPerform test 1 (IP header)\n\
\t\t-2 \t\tPerform test 2 (IP options)\n\
\t\t-3 \t\tPerform test 3 (ICMP)\n\
\t\t-4 \t\tPerform test 4 (UDP)\n\
\t\t-5 \t\tPerform test 5 (TCP)\n\
", prog);
	exit(1);
}


void do_icmp(ip, args)
ip_t *ip;
char *args;
{
	struct	icmp	*ic;
	char	*s;

	ip->ip_p = IPPROTO_ICMP;
	ip->ip_len += sizeof(*ic);
	ic = (struct icmp *)(ip + 1);
	bzero((char *)ic, sizeof(*ic));
	if (!(s = strchr(args, ',')))
	    {
		fprintf(stderr, "ICMP args missing: ,\n");
		return;
	    }
	*s++ = '\0';
	ic->icmp_type = atoi(args);
	ic->icmp_code = atoi(s);
	if (ic->icmp_type == ICMP_REDIRECT && strchr(s, ','))
	    {
		char	*t;

		t = strtok(s, ",");
		t = strtok(NULL, ",");
		if (resolve(t, (char *)&ic->icmp_gwaddr) == -1)
		    {
			fprintf(stderr,"Cant resolve %s\n", t);
			exit(2);
		    }
		if ((t = strtok(NULL, ",")))
		    {
			if (resolve(t, (char *)&ic->icmp_ip.ip_dst) == -1)
			    {
				fprintf(stderr,"Cant resolve %s\n", t);
				exit(2);
			    }
			if ((t = strtok(NULL, ",")))
			    {
				if (resolve(t,
					    (char *)&ic->icmp_ip.ip_src) == -1)
				    {
					fprintf(stderr,"Cant resolve %s\n", t);
					exit(2);
				    }
			    }
		    }
	    }
}


int send_packets(dev, mtu, ip, gwip)
char *dev;
int mtu;
ip_t *ip;
struct in_addr gwip;
{
	u_short	sport = 0;
	int	wfd;

	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
		sport = ((struct tcpiphdr *)ip)->ti_sport;
	wfd = initdevice(dev, sport, 5);

	return send_packet(wfd, mtu, ip, gwip);
}


main(argc, argv)
int	argc;
char	**argv;
{
	struct	tcpiphdr *ti;
	struct	in_addr	gwip;
	struct	ipread	*ipr = NULL;
	tcphdr_t	*tcp;
	ip_t	*ip;
	char	*name =  argv[0], host[64], *gateway = NULL, *dev = NULL;
	char	*src = NULL, *dst, c, *s, *resend = NULL;
	int	mtu = 1500, olen = 0, tests = 0, pointtest = 0;

	/*
	 * 65535 is maximum packet size...you never know...
	 */
	ip = (ip_t *)calloc(1, 65536);
	ti = (struct tcpiphdr *)ip;
	tcp = (tcphdr_t *)&ti->ti_sport;
	ip->ip_len = sizeof(*ip);
	ip->ip_hl = sizeof(*ip) >> 2;

	while ((c = getopt(argc, argv, "12345IP:R:TUd:f:g:m:o:p:r:s:t:")) != -1)
		switch (c)
		{
		case '1' :
		case '2' :
		case '3' :
		case '4' :
		case '5' :
			tests = c - '0';
			break;
		case 'I' :
			if (ip->ip_p)
			    {
				fprintf(stderr, "Protocol already set: %d\n",
					ip->ip_p);
				break;
			    }
			do_icmp(ip, optarg);
			break;
		case 'P' :
		    {
			struct	protoent	*p;

			if (ip->ip_p)
			    {
				fprintf(stderr, "Protocol already set: %d\n",
					ip->ip_p);
				break;
			    }
			if ((p = getprotobyname(optarg)))
				ip->ip_p = p->p_proto;
			else
				fprintf(stderr, "Unknown protocol: %s\n",
					optarg);
			break;
		    }
		case 'R' :
			resend = optarg;
			ipr = &pcap;
			break;
		case 'T' :
			if (ip->ip_p)
			    {
				fprintf(stderr, "Protocol already set: %d\n",
					ip->ip_p);
				break;
			    }
			ip->ip_p = IPPROTO_TCP;
			ip->ip_len += sizeof(tcphdr_t);
			break;
		case 'U' :
			if (ip->ip_p)
			    {
				fprintf(stderr, "Protocol already set: %d\n",
					ip->ip_p);
				break;
			    }
			ip->ip_p = IPPROTO_UDP;
			ip->ip_len += sizeof(udphdr_t);
			break;
		case 'd' :
			dev = optarg;
			break;
		case 'f' :
			ip->ip_off = strtol(optarg, NULL, 0);
			break;
		case 'g' :
			gateway = optarg;
			break;
		case 'm' :
			mtu = atoi(optarg);
			if (mtu < 28)
			    {
				fprintf(stderr, "mtu must be > 28\n");
				exit(1);
			    }
			break;
		case 'o' :
			olen = optname(optarg, options);
			break;
		case 'p' :
			pointtest = atoi(optarg);
			break;
		case 'r' :
			resend = optarg;
			ipr = &pcap;
			break;
		case 's' :
			src = optarg;
			break;
		case 't' :
			if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
				tcp->th_dport = htons(atoi(optarg));
			break;
		case 'w' :
			if (ip->ip_p == IPPROTO_TCP)
				tcp->th_win = atoi(optarg);
			else
				fprintf(stderr, "set protocol to TCP first\n");
			break;
		default :
			fprintf(stderr, "Unknown option \"%c\"\n", c);
			usage(name);
		}

	if (ipr)
	    {
		if (!gateway)
			usage(name);
		dst = gateway;
	    }
	else
	    {
		if (argc - optind < 2 && !tests)
			usage(name);
		dst = argv[optind++];
	    }

	if (!src)
	    {
		gethostname(host, sizeof(host));
		src = host;
	    }

	if (resolve(src, (char *)&ip->ip_src) == -1)
	    {
		fprintf(stderr,"Cant resolve %s\n", src);
		exit(2);
	    }

	if (resolve(dst, (char *)&ip->ip_dst) == -1)
	    {
		fprintf(stderr,"Cant resolve %s\n", dst);
		exit(2);
	    }

	if (!gateway)
		gwip = ip->ip_dst;
	else if (resolve(gateway, (char *)&gwip) == -1)
	    {
		fprintf(stderr,"Cant resolve %s\n", src);
		exit(2);
	    }

	if (!ipr && ip->ip_p == IPPROTO_TCP)
		for (s = argv[optind]; c = *s; s++)
			switch(c)
			{
			case 'S' : case 's' :
				tcp->th_flags |= TH_SYN;
				break;
			case 'A' : case 'a' :
				tcp->th_flags |= TH_ACK;
				break;
			case 'F' : case 'f' :
				tcp->th_flags |= TH_FIN;
				break;
			case 'R' : case 'r' :
				tcp->th_flags |= TH_RST;
				break;
			case 'P' : case 'p' :
				tcp->th_flags |= TH_PUSH;
				break;
			case 'U' : case 'u' :
				tcp->th_flags |= TH_URG;
				break;
			}

	if (!dev) {
#if defined (__OpenBSD__) || defined (__NetBSD__)
		char errbuf[160];
		if (! (dev = pcap_lookupdev(errbuf))) {
			fprintf (stderr, "%s", errbuf);
			dev = default_device;
		}
#else
		dev = default_device;
#endif
	}
	printf("Device:  %s\n", dev);
	printf("Source:  %s\n", inet_ntoa(ip->ip_src));
	printf("Dest:    %s\n", inet_ntoa(ip->ip_dst));
	printf("Gateway: %s\n", inet_ntoa(gwip));
	if (ip->ip_p == IPPROTO_TCP && tcp->th_flags)
		printf("Flags:   %#x\n", tcp->th_flags);
	printf("mtu:     %d\n", mtu);

	if (olen)
	    {
		printf("Options: %d\n", olen);
		ti = (struct tcpiphdr *)malloc(olen + ip->ip_len);
		bcopy((char *)ip, (char *)ti, sizeof(*ip));
		ip = (ip_t *)ti;
		ip->ip_hl += (olen >> 2);
		bcopy(options, (char *)(ip + 1), olen);
		bcopy((char *)tcp, (char *)(ip + 1) + olen, sizeof(*tcp));
		tcp = (tcphdr_t *)((char *)(ip + 1) + olen);
		ip->ip_len += olen;
	    }

	switch (tests)
	{
	case 1 :
		return ip_test1(dev, mtu, ti, gwip, pointtest);
	case 2 :
		return ip_test2(dev, mtu, ti, gwip, pointtest);
	case 3 :
		return ip_test3(dev, mtu, ti, gwip, pointtest);
	case 4 :
		return ip_test4(dev, mtu, ti, gwip, pointtest);
	case 5 :
		return ip_test5(dev, mtu, ti, gwip, pointtest);
	default :
		break;
	}

	if (ipr)
		return ip_resend(dev, mtu, ipr, gwip, resend);

#ifdef	DOSOCKET
	if (tcp->th_dport)
		return do_socket(dev, mtu, ti, gwip);
#endif
	return send_packets(dev, mtu, ti, gwip);
}
