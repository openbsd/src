/*	$OpenBSD: brconfig.c,v 1.31 2004/05/23 06:47:49 deraadt Exp $	*/

/*
 * Copyright (c) 1999, 2000 Jason L. Wright (jason@thought.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/if_bridge.h>
#include <sys/errno.h>
#include <string.h>
#include <err.h>
#include <sysexits.h>
#include <limits.h>

void usage(void);
int bridge_setflag(int, char *, short);
int bridge_clrflag(int, char *, short);
int bridge_ifsetflag(int, char *, char *, u_int32_t);
int bridge_ifclrflag(int, char *, char *, u_int32_t);
int bridge_list(int, char *, char *);
int bridge_cfg(int, char *, char *);
int bridge_addrs(int, char *, char *);
int bridge_addaddr(int, char *, char *, char *);
int bridge_deladdr(int, char *, char *);
int bridge_maxaddr(int, char *, char *);
int bridge_maxage(int, char *, char *);
int bridge_priority(int, char *, char *);
int bridge_fwddelay(int, char *, char *);
int bridge_hellotime(int, char *, char *);
int bridge_ifprio(int, char *, char *, char *);
int bridge_ifcost(int, char *, char *, char *);
int bridge_timeout(int, char *, char *);
int bridge_flush(int, char *);
int bridge_flushall(int, char *);
int bridge_add(int, char *, char *);
int bridge_delete(int, char *, char *);
int bridge_addspan(int, char *, char *);
int bridge_delspan(int, char *, char *);
int bridge_status(int, char *);
int is_bridge(int, char *);
int bridge_show_all(int);
void printb(char *, unsigned short, char *);
int bridge_rule(int, char *, int, char **, int);
int bridge_rules(int, char *, char *, char *);
int bridge_flushrule(int, char *, char *);
void bridge_badrule(int, char **, int);
void bridge_showrule(struct ifbrlreq *, char *);
int bridge_rulefile(int, char *, char *);

/* if_flags bits: borrowed from ifconfig.c */
#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6NOTRAILERS\7RUNNING\10NOARP\
\11PROMISC\12ALLMULTI\13OACTIVE\14SIMPLEX\15LINK0\16LINK1\17LINK2\20MULTICAST"

#define	IFBAFBITS	"\020\1STATIC"
#define	IFBIFBITS	"\020\1LEARNING\2DISCOVER\3BLOCKNONIP\4STP\11SPAN"

char *stpstates[] = {
	"disabled",
	"listening",
	"learning",
	"forwarding",
	"blocking",
};

void
usage(void)
{
	fprintf(stderr, "usage: brconfig -a\n");
	fprintf(stderr,
	    "       brconfig interface [up] [down] [add if] [del if] ...\n");
}

int
main(int argc, char *argv[])
{
	int error = 0, sock;
	char *brdg;

	if (argc < 2) {
		usage();
		return (EX_USAGE);
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		err(1, "socket");

	argc--; argv++;
	brdg = argv[0];

	if (strcmp(brdg, "-a") == 0)
		return bridge_show_all(sock);

	if (strlen(brdg) >= IFNAMSIZ) {
		warnx("%s is not a bridge", brdg);
		return (EX_USAGE);
	}

	if (!is_bridge(sock, brdg)) {
		if (errno == ENXIO)
			warn("%s", brdg);
		else
			warnx("%s is not a bridge", brdg);
		return (EX_USAGE);
	}

	if (argc == 1) {
		error = bridge_status(sock, brdg);
		return (error);
	}

	for (argc--, argv++; argc != 0; argc--, argv++) {
		if (strcmp("add", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("add requires an argument");
				return (EX_USAGE);
			}
			error = bridge_add(sock, brdg, argv[0]);
			if (error)
				return (error);
		} else if (strcmp("delete", argv[0]) == 0 ||
		    strcmp("del", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("delete requires an argument");
				return (EX_USAGE);
			}
			error = bridge_delete(sock, brdg, argv[0]);
			if (error)
				return (error);
		} else if (strcmp("addspan", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("addspan requires an argument");
				return (EX_USAGE);
			}
			error = bridge_addspan(sock, brdg, argv[0]);
			if (error)
				return (error);
		} else if (strcmp("delspan", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("delspan requires an argument");
				return (EX_USAGE);
			}
			error = bridge_delspan(sock, brdg, argv[0]);
			if (error)
				return (error);
		} else if (strcmp("up", argv[0]) == 0) {
			error = bridge_setflag(sock, brdg, IFF_UP);
			if (error)
				return (error);
		} else if (strcmp("down", argv[0]) == 0) {
			error = bridge_clrflag(sock, brdg, IFF_UP);
			if (error)
				return (error);
		} else if (strcmp("discover", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("discover requires an argument");
				return (EX_USAGE);
			}
			error = bridge_ifsetflag(sock, brdg, argv[0],
			    IFBIF_DISCOVER);
			if (error)
				return (error);
		} else if (strcmp("-discover", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("-discover requires an argument");
				return (EX_USAGE);
			}
			error = bridge_ifclrflag(sock, brdg, argv[0],
			    IFBIF_DISCOVER);
			if (error)
				return (error);
		} else if (strcmp("blocknonip", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("blocknonip requires an argument");
				return (EX_USAGE);
			}
			error = bridge_ifsetflag(sock, brdg, argv[0],
			    IFBIF_BLOCKNONIP);
			if (error)
				return (error);
		} else if (strcmp("-blocknonip", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("-blocknonip requires an argument");
				return (EX_USAGE);
			}
			error = bridge_ifclrflag(sock, brdg, argv[0],
			    IFBIF_BLOCKNONIP);
			if (error)
				return (error);
		} else if (strcmp("learn", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("learn requires an argument");
				return (EX_USAGE);
			}
			error = bridge_ifsetflag(sock, brdg, argv[0],
			    IFBIF_LEARNING);
			if (error)
				return (error);
		} else if (strcmp("-learn", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("-learn requires an argument");
				return (EX_USAGE);
			}
			error = bridge_ifclrflag(sock, brdg, argv[0],
			    IFBIF_LEARNING);
			if (error)
				return (error);
		} else if (strcmp("flush", argv[0]) == 0) {
			error = bridge_flush(sock, brdg);
			if (error)
				return (error);
		} else if (strcmp("flushall", argv[0]) == 0) {
			error = bridge_flushall(sock, brdg);
			if (error)
				return (error);
		} else if (strcmp("static", argv[0]) == 0) {
			argc--; argv++;
			if (argc < 2) {
				warnx("static requires 2 arguments");
				return (EX_USAGE);
			}
			error = bridge_addaddr(sock, brdg, argv[0], argv[1]);
			if (error)
				return (error);
			argc--; argv++;
		} else if (strcmp("deladdr", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("deladdr requires an argument");
				return (EX_USAGE);
			}
			error = bridge_deladdr(sock, brdg, argv[0]);
			if (error)
				return (error);
		} else if (strcmp("link0", argv[0]) == 0) {
			error = bridge_setflag(sock, brdg, IFF_LINK0);
			if (error)
				return (error);
		} else if (strcmp("-link0", argv[0]) == 0) {
			error = bridge_clrflag(sock, brdg, IFF_LINK0);
			if (error)
				return (error);
		} else if (strcmp("link1", argv[0]) == 0) {
			error = bridge_setflag(sock, brdg, IFF_LINK1);
			if (error)
				return (error);
		} else if (strcmp("-link1", argv[0]) == 0) {
			error = bridge_clrflag(sock, brdg, IFF_LINK1);
			if (error)
				return (error);
		} else if (strcmp("link2", argv[0]) == 0) {
			error = bridge_setflag(sock, brdg, IFF_LINK2);
			if (error)
				return (error);
		} else if (strcmp("-link2", argv[0]) == 0) {
			error = bridge_clrflag(sock, brdg, IFF_LINK2);
			if (error)
				return (error);
		} else if (strcmp("addr", argv[0]) == 0) {
			error = bridge_addrs(sock, brdg, "");
			if (error)
				return (error);
		} else if (strcmp("maxaddr", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("maxaddr requires an argument");
				return (EX_USAGE);
			}
			error = bridge_maxaddr(sock, brdg, argv[0]);
			if (error)
				return (error);
		} else if (strcmp("hellotime", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("hellotime requires an argument");
				return (EX_USAGE);
			}
			error = bridge_hellotime(sock, brdg, argv[0]);
			if (error)
				return (error);
		} else if (strcmp("fwddelay", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("fwddelay requires an argument");
				return (EX_USAGE);
			}
			error = bridge_fwddelay(sock, brdg, argv[0]);
			if (error)
				return (error);
		} else if (strcmp("maxage", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("maxage requires an argument");
				return (EX_USAGE);
			}
			error = bridge_maxage(sock, brdg, argv[0]);
			if (error)
				return (error);
		} else if (strcmp("priority", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("priority requires an argument");
				return (EX_USAGE);
			}
			error = bridge_priority(sock, brdg, argv[0]);
			if (error)
				return (error);
		} else if (strcmp("ifpriority", argv[0]) == 0) {
			argc--; argv++;
			if (argc < 2) {
				warnx("ifpriority requires 2 arguments");
				return (EX_USAGE);
			}
			error = bridge_ifprio(sock, brdg, argv[0], argv[1]);
			if (error)
				return (error);
			argc--; argv++;
		} else if (strcmp("ifcost", argv[0]) == 0) {
			argc--; argv++;
			if (argc < 2) {
				warnx("ifcost requires 2 arguments");
				return (EX_USAGE);
			}
			error = bridge_ifcost(sock, brdg, argv[0], argv[1]);
			if (error)
				return (error);
			argc--; argv++;
		} else if (strcmp("rules", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("rules requires an argument");
				return (EX_USAGE);
			}
			error = bridge_rules(sock, brdg, argv[0], NULL);
			if (error)
				return (error);
		} else if (strcmp("rule", argv[0]) == 0) {
			argc--; argv++;
			return (bridge_rule(sock, brdg, argc, argv, -1));
		} else if (strcmp("rulefile", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("rulefile requires an argument");
				return (EX_USAGE);
			}
			error = bridge_rulefile(sock, brdg, argv[0]);
			if (error)
				return (error);
		} else if (strcmp("flushrule", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("flushrule requires an argument");
				return (EX_USAGE);
			}
			error = bridge_flushrule(sock, brdg, argv[0]);
			if (error)
				return (error);
		} else if (strcmp("timeout", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("timeout requires an argument");
				return (EX_USAGE);
			}
			error = bridge_timeout(sock, brdg, argv[0]);
			if (error)
				return (error);
		} else if (strcmp("stp", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("stp requires an argument");
				return (EX_USAGE);
			}
			error = bridge_ifsetflag(sock, brdg, argv[0],
			    IFBIF_STP);
			if (error)
				return (error);
		} else if (strcmp("-stp", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("-stp requires an argument");
				return (EX_USAGE);
			}
			error = bridge_ifclrflag(sock, brdg, argv[0],
			    IFBIF_STP);
			if (error)
				return (error);
		} else {
			warnx("unrecognized option: %s", argv[0]);
			return (EX_USAGE);
		}
	}

	return (0);
}

int
bridge_ifsetflag(int s, char *brdg, char *ifsname, u_int32_t flag)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, brdg, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifsname, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGGIFFLGS, (caddr_t)&req) < 0) {
		warn("%s: %s", brdg, ifsname);
		return (EX_IOERR);
	}

	req.ifbr_ifsflags |= flag;

	if (ioctl(s, SIOCBRDGSIFFLGS, (caddr_t)&req) < 0) {
		warn("%s: %s", brdg, ifsname);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_ifclrflag(int s, char *brdg, char *ifsname, u_int32_t flag)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, brdg, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifsname, sizeof(req.ifbr_ifsname));

	if (ioctl(s, SIOCBRDGGIFFLGS, (caddr_t)&req) < 0) {
		warn("%s: %s", brdg, ifsname);
		return (EX_IOERR);
	}

	req.ifbr_ifsflags &= ~flag;

	if (ioctl(s, SIOCBRDGSIFFLGS, (caddr_t)&req) < 0) {
		warn("%s: %s", brdg, ifsname);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_show_all(int s)
{
	char *inbuf = NULL, *inb;
	struct ifconf ifc;
	struct ifreq *ifrp, ifreq;
	int len = 8192, i;

	while (1) {
		ifc.ifc_len = len;
		inb = realloc(inbuf, len);
		if (inb == NULL)
			err(1, "malloc");
		ifc.ifc_buf = inbuf = inb;
		if (ioctl(s, SIOCGIFCONF, &ifc) < 0)
			err(1, "ioctl(SIOCGIFCONF)");
		if (ifc.ifc_len + sizeof(struct ifreq) < len)
			break;
		len *= 2;
	}
	ifrp = ifc.ifc_req;
	ifreq.ifr_name[0] = '\0';
	for (i = 0; i < ifc.ifc_len; ) {
		ifrp = (struct ifreq *)((caddr_t)ifc.ifc_req + i);
		i += sizeof(ifrp->ifr_name) +
		    (ifrp->ifr_addr.sa_len > sizeof(struct sockaddr) ?
		    ifrp->ifr_addr.sa_len : sizeof(struct sockaddr));
		if (ifrp->ifr_addr.sa_family != AF_LINK)
			continue;
		if (!is_bridge(s, ifrp->ifr_name))
			continue;
		bridge_status(s, ifrp->ifr_name);
	}
	free(inbuf);
	return (0);
}

int
bridge_setflag(int s, char *brdg, short f)
{
	struct ifreq ifr;

	strlcpy(ifr.ifr_name, brdg, sizeof(ifr.ifr_name));

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("%s", brdg);
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	ifr.ifr_flags |= f;

	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("%s", brdg);
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	return (0);
}

int
bridge_clrflag(int s, char *brdg, short f)
{
	struct ifreq ifr;

	strlcpy(ifr.ifr_name, brdg, sizeof(ifr.ifr_name));

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("%s", brdg);
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	ifr.ifr_flags &= ~(f);

	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("%s", brdg);
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	return (0);
}

int
bridge_flushall(int s, char *brdg)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, brdg, sizeof(req.ifbr_name));
	req.ifbr_ifsflags = IFBF_FLUSHALL;
	if (ioctl(s, SIOCBRDGFLUSH, &req) < 0) {
		warn("%s", brdg);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_flush(int s, char *brdg)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, brdg, sizeof(req.ifbr_name));
	req.ifbr_ifsflags = IFBF_FLUSHDYN;
	if (ioctl(s, SIOCBRDGFLUSH, &req) < 0) {
		warn("%s", brdg);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_cfg(int s, char *brdg, char *delim)
{
	struct ifbrparam ifbp;
	u_int16_t pri;
	u_int8_t ht, fd, ma;

	strlcpy(ifbp.ifbrp_name, brdg, sizeof(ifbp.ifbrp_name));
	if (ioctl(s, SIOCBRDGGPRI, (caddr_t)&ifbp)) {
		warn("%s", brdg);
		return (EX_IOERR);
	}
	pri = ifbp.ifbrp_prio;

	if (ioctl(s, SIOCBRDGGHT, (caddr_t)&ifbp)) {
		warn("%s", brdg);
		return (EX_IOERR);
	}
	ht = ifbp.ifbrp_hellotime;

	if (ioctl(s, SIOCBRDGGFD, (caddr_t)&ifbp)) {
		warn("%s", brdg);
		return (EX_IOERR);
	}
	fd = ifbp.ifbrp_fwddelay;

	if (ioctl(s, SIOCBRDGGMA, (caddr_t)&ifbp)) {
		warn("%s", brdg);
		return (EX_IOERR);
	}
	ma = ifbp.ifbrp_maxage;

	printf("%spriority %u hellotime %u fwddelay %u maxage %u\n",
	    delim, pri, ht, fd, ma);

	return (0);
}

int
bridge_list(int s, char *brdg, char *delim)
{
	struct ifbreq *reqp;
	struct ifbifconf bifc;
	int i, len = 8192;
	char buf[sizeof(reqp->ifbr_ifsname) + 1], *inbuf = NULL, *inb;

	while (1) {
		bifc.ifbic_len = len;
		inb = realloc(inbuf, len);
		if (inb == NULL)
			err(1, "malloc");
		bifc.ifbic_buf = inbuf = inb;
		strlcpy(bifc.ifbic_name, brdg, sizeof(bifc.ifbic_name));
		if (ioctl(s, SIOCBRDGIFS, &bifc) < 0)
			err(1, "%s", brdg);
		if (bifc.ifbic_len + sizeof(*reqp) < len)
			break;
		len *= 2;
	}
	for (i = 0; i < bifc.ifbic_len / sizeof(*reqp); i++) {
		reqp = bifc.ifbic_req + i;
		strlcpy(buf, reqp->ifbr_ifsname, sizeof(buf));
		printf("%s%s ", delim, buf);
		printb("flags", reqp->ifbr_ifsflags, IFBIFBITS);
		printf("\n");
		if (reqp->ifbr_ifsflags & IFBIF_SPAN)
			continue;
		printf("\t\t\t");
		printf("port %u ifpriority %u ifcost %u",
		    reqp->ifbr_portno, reqp->ifbr_priority,
		    reqp->ifbr_path_cost);
		if (reqp->ifbr_ifsflags & IFBIF_STP)
			printf(" %s", stpstates[reqp->ifbr_state]);
		printf("\n");
		bridge_rules(s, brdg, buf, delim);
	}
	free(bifc.ifbic_buf);
	return (0);		/* NOTREACHED */
}

int
bridge_add(int s, char *brdg, char *ifn)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, brdg, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGADD, &req) < 0) {
		warn("%s: %s", brdg, ifn);
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_delete(int s, char *brdg, char *ifn)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, brdg, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGDEL, &req) < 0) {
		warn("%s: %s", brdg, ifn);
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_addspan(int s, char *brdg, char *ifn)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, brdg, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGADDS, &req) < 0) {
		warn("%s: %s", brdg, ifn);
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_delspan(int s, char *brdg, char *ifn)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, brdg, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGDELS, &req) < 0) {
		warn("%s: %s", brdg, ifn);
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_timeout(int s, char *brdg, char *arg)
{
	struct ifbrparam bp;
	long newtime;
	char *endptr;

	errno = 0;
	newtime = strtol(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0' ||
	    (newtime & ~INT_MAX) != 0L ||
	    (errno == ERANGE && newtime == LONG_MAX)) {
		printf("invalid arg for timeout: %s\n", arg);
		return (EX_USAGE);
	}

	strlcpy(bp.ifbrp_name, brdg, sizeof(bp.ifbrp_name));
	bp.ifbrp_ctime = newtime;
	if (ioctl(s, SIOCBRDGSTO, (caddr_t)&bp) < 0) {
		warn("%s", brdg);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_maxage(int s, char *brdg, char *arg)
{
	struct ifbrparam bp;
	unsigned long v;
	char *endptr;

	errno = 0;
	v = strtoul(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0' || v > 0xffUL ||
	    (errno == ERANGE && v == ULONG_MAX)) {
		printf("invalid arg for maxage: %s\n", arg);
		return (EX_USAGE);
	}

	strlcpy(bp.ifbrp_name, brdg, sizeof(bp.ifbrp_name));
	bp.ifbrp_maxage = v;
	if (ioctl(s, SIOCBRDGSMA, (caddr_t)&bp) < 0) {
		warn("%s", brdg);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_priority(int s, char *brdg, char *arg)
{
	struct ifbrparam bp;
	unsigned long v;
	char *endptr;

	errno = 0;
	v = strtoul(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0' || v > 0xffffUL ||
	    (errno == ERANGE && v == ULONG_MAX)) {
		printf("invalid arg for maxage: %s\n", arg);
		return (EX_USAGE);
	}

	strlcpy(bp.ifbrp_name, brdg, sizeof(bp.ifbrp_name));
	bp.ifbrp_prio = v;
	if (ioctl(s, SIOCBRDGSPRI, (caddr_t)&bp) < 0) {
		warn("%s", brdg);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_fwddelay(int s, char *brdg, char *arg)
{
	struct ifbrparam bp;
	unsigned long v;
	char *endptr;

	errno = 0;
	v = strtoul(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0' || v > 0xffUL ||
	    (errno == ERANGE && v == ULONG_MAX)) {
		printf("invalid arg for fwddelay: %s\n", arg);
		return (EX_USAGE);
	}

	strlcpy(bp.ifbrp_name, brdg, sizeof(bp.ifbrp_name));
	bp.ifbrp_fwddelay = v;
	if (ioctl(s, SIOCBRDGSFD, (caddr_t)&bp) < 0) {
		warn("%s", brdg);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_hellotime(int s, char *brdg, char *arg)
{
	struct ifbrparam bp;
	unsigned long v;
	char *endptr;

	errno = 0;
	v = strtoul(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0' || v > 0xffUL ||
	    (errno == ERANGE && v == ULONG_MAX)) {
		printf("invalid arg for hellotime: %s\n", arg);
		return (EX_USAGE);
	}

	strlcpy(bp.ifbrp_name, brdg, sizeof(bp.ifbrp_name));
	bp.ifbrp_hellotime = v;
	if (ioctl(s, SIOCBRDGSHT, (caddr_t)&bp) < 0) {
		warn("%s", brdg);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_maxaddr(int s, char *brdg, char *arg)
{
	struct ifbrparam bp;
	unsigned long newsize;
	char *endptr;

	errno = 0;
	newsize = strtoul(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0' || newsize > 0xffffffffUL ||
	    (errno == ERANGE && newsize == ULONG_MAX)) {
		printf("invalid arg for maxaddr: %s\n", arg);
		return (EX_USAGE);
	}

	strlcpy(bp.ifbrp_name, brdg, sizeof(bp.ifbrp_name));
	bp.ifbrp_csize = newsize;
	if (ioctl(s, SIOCBRDGSCACHE, (caddr_t)&bp) < 0) {
		warn("%s", brdg);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_deladdr(int s, char *brdg, char *addr)
{
	struct ifbareq ifba;
	struct ether_addr *ea;

	strlcpy(ifba.ifba_name, brdg, sizeof(ifba.ifba_name));
	ea = ether_aton(addr);
	if (ea == NULL) {
		warnx("Invalid address: %s", addr);
		return (EX_USAGE);
	}
	bcopy(ea, &ifba.ifba_dst, sizeof(struct ether_addr));

	if (ioctl(s, SIOCBRDGDADDR, &ifba) < 0) {
		warn("%s: %s", brdg, addr);
		return (EX_IOERR);
	}

	return (0);
}

int
bridge_ifprio(int s, char *brdg, char *ifname, char *val)
{
	struct ifbreq breq;
	unsigned long v;
	char *endptr;

	strlcpy(breq.ifbr_name, brdg, sizeof(breq.ifbr_name));
	strlcpy(breq.ifbr_ifsname, ifname, sizeof(breq.ifbr_ifsname));

	errno = 0;
	v = strtoul(val, &endptr, 0);
	if (val[0] == '\0' || endptr[0] != '\0' || v > 0xffUL ||
	    (errno == ERANGE && v == ULONG_MAX)) {
		printf("invalid arg for ifpriority: %s\n", val);
		return (EX_USAGE);
	}
	breq.ifbr_priority = v;

	if (ioctl(s, SIOCBRDGSIFPRIO, (caddr_t)&breq) < 0) {
		warn("%s: %s", brdg, val);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_ifcost(int s, char *brdg, char *ifname, char *val)
{
	struct ifbreq breq;
	unsigned long v;
	char *endptr;

	strlcpy(breq.ifbr_name, brdg, sizeof(breq.ifbr_name));
	strlcpy(breq.ifbr_ifsname, ifname, sizeof(breq.ifbr_ifsname));

	errno = 0;
	v = strtoul(val, &endptr, 0);
	if (val[0] == '\0' || endptr[0] != '\0' ||
	    v < 1 || v > 0xffffffffUL ||
	    (errno == ERANGE && v == ULONG_MAX)) {
		printf("invalid arg for ifcost: %s\n", val);
		return (EX_USAGE);
	}
	breq.ifbr_path_cost = v;

	if (ioctl(s, SIOCBRDGSIFCOST, (caddr_t)&breq) < 0) {
		warn("%s: %s", brdg, val);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_addaddr(int s, char *brdg, char *ifname, char *addr)
{
	struct ifbareq ifba;
	struct ether_addr *ea;

	strlcpy(ifba.ifba_name, brdg, sizeof(ifba.ifba_name));
	strlcpy(ifba.ifba_ifsname, ifname, sizeof(ifba.ifba_ifsname));

	ea = ether_aton(addr);
	if (ea == NULL) {
		warnx("Invalid address: %s", addr);
		return (EX_USAGE);
	}
	bcopy(ea, &ifba.ifba_dst, sizeof(struct ether_addr));
	ifba.ifba_flags = IFBAF_STATIC;

	if (ioctl(s, SIOCBRDGSADDR, &ifba) < 0) {
		warn("%s: %s", brdg, addr);
		return (EX_IOERR);
	}

	return (0);
}

int
bridge_addrs(int s, char *brdg, char *delim)
{
	struct ifbaconf ifbac;
	struct ifbareq *ifba;
	char *inbuf = NULL, buf[sizeof(ifba->ifba_ifsname) + 1], *inb;
	int i, len = 8192;

	while (1) {
		ifbac.ifbac_len = len;
		inb = realloc(inbuf, len);
		if (inb == NULL)
			err(EX_IOERR, "malloc");
		ifbac.ifbac_buf = inbuf = inb;
		strlcpy(ifbac.ifbac_name, brdg, sizeof(ifbac.ifbac_name));
		if (ioctl(s, SIOCBRDGRTS, &ifbac) < 0) {
			if (errno == ENETDOWN)
				return (0);
			err(EX_IOERR, "%s", brdg);
		}
		if (ifbac.ifbac_len + sizeof(*ifba) < len)
			break;
		len *= 2;
	}

	for (i = 0; i < ifbac.ifbac_len / sizeof(*ifba); i++) {
		ifba = ifbac.ifbac_req + i;
		strlcpy(buf, ifba->ifba_ifsname, sizeof(buf));
		printf("%s%s %s %u ", delim, ether_ntoa(&ifba->ifba_dst),
		    buf, ifba->ifba_age);
		printb("flags", ifba->ifba_flags, IFBAFBITS);
		printf("\n");
	}
	free(inbuf);
	return (0);
}

/*
 * Check to make sure 'brdg' is really a bridge interface.
 */
int
is_bridge(int s, char *brdg)
{
	struct ifreq ifr;
	struct ifbaconf ifbac;

	strlcpy(ifr.ifr_name, brdg, sizeof(ifr.ifr_name));

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0)
		return (0);

	ifbac.ifbac_len = 0;
	strlcpy(ifbac.ifbac_name, brdg, sizeof(ifbac.ifbac_name));
	if (ioctl(s, SIOCBRDGRTS, (caddr_t)&ifbac) < 0) {
		if (errno == ENETDOWN)
			return (1);
		return (0);
	}
	return (1);
}

int
bridge_status(int s, char *brdg)
{
	struct ifreq ifr;
	struct ifbrparam bp1, bp2;
	int err;

	strlcpy(ifr.ifr_name, brdg, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("%s", brdg);
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	printf("%s: ", brdg);
	printb("flags", ifr.ifr_flags, IFFBITS);
	printf("\n");

	printf("\tConfiguration:\n");
	err = bridge_cfg(s, brdg, "\t\t");
	if (err)
		return (err);

	printf("\tInterfaces:\n");
	err = bridge_list(s, brdg, "\t\t");
	if (err)
		return (err);

	strlcpy(bp1.ifbrp_name, brdg, sizeof(bp1.ifbrp_name));
	if (ioctl(s, SIOCBRDGGCACHE, (caddr_t)&bp1) < 0) {
		warn("%s", brdg);
		return (EX_IOERR);
	}

	strlcpy(bp2.ifbrp_name, brdg, sizeof(bp2.ifbrp_name));
	if (ioctl(s, SIOCBRDGGTO, (caddr_t)&bp2) < 0) {
		warn("%s", brdg);
		return (EX_IOERR);
	}

	printf("\tAddresses (max cache: %u, timeout: %u):\n",
	    bp1.ifbrp_csize, bp2.ifbrp_ctime);

	err = bridge_addrs(s, brdg, "\t\t");
	return (err);
}

int
bridge_flushrule(int s, char *brdg, char *ifname)
{
	struct ifbrlreq req;

	strlcpy(req.ifbr_name, brdg, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifname, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGFRL, &req) < 0) {
		warn("%s: %s", brdg, ifname);
		return (EX_USAGE);
	}
	return (0);
}

int
bridge_rules(int s, char *brdg, char *ifname, char *delim)
{
	char *inbuf = NULL, *inb;
	struct ifbrlconf ifc;
	struct ifbrlreq *ifrp, ifreq;
	int len = 8192, i;

	while (1) {
		ifc.ifbrl_len = len;
		inb = realloc(inbuf, len);
		if (inb == NULL)
			err(1, "malloc");
		ifc.ifbrl_buf = inbuf = inb;
		strlcpy(ifc.ifbrl_name, brdg, sizeof(ifc.ifbrl_name));
		strlcpy(ifc.ifbrl_ifsname, ifname, sizeof(ifc.ifbrl_ifsname));
		if (ioctl(s, SIOCBRDGGRL, &ifc) < 0)
			err(1, "ioctl(SIOCBRDGGRL)");
		if (ifc.ifbrl_len + sizeof(ifreq) < len)
			break;
		len *= 2;
	}
	ifrp = ifc.ifbrl_req;
	for (i = 0; i < ifc.ifbrl_len; i += sizeof(ifreq)) {
		ifrp = (struct ifbrlreq *)((caddr_t)ifc.ifbrl_req + i);
		bridge_showrule(ifrp, delim);
	}
	return (0);
}

void
bridge_showrule(struct ifbrlreq *r, char *delim)
{
	if (delim)
		printf("%s    ", delim);
	else
		printf("%s: ", r->ifbr_name);

	if (r->ifbr_action == BRL_ACTION_BLOCK)
		printf("block ");
	else if (r->ifbr_action == BRL_ACTION_PASS)
		printf("pass ");
	else
		printf("[neither block nor pass?]\n");

	if ((r->ifbr_flags & (BRL_FLAG_IN | BRL_FLAG_OUT)) ==
	    (BRL_FLAG_IN | BRL_FLAG_OUT))
		printf("in/out ");
	else if (r->ifbr_flags & BRL_FLAG_IN)
		printf("in ");
	else if (r->ifbr_flags & BRL_FLAG_OUT)
		printf("out ");
	else
		printf("[neither in nor out?]\n");

	printf("on %s", r->ifbr_ifsname);

	if (r->ifbr_flags & BRL_FLAG_SRCVALID)
		printf(" src %s", ether_ntoa(&r->ifbr_src));
	if (r->ifbr_flags & BRL_FLAG_DSTVALID)
		printf(" dst %s", ether_ntoa(&r->ifbr_dst));
	if (r->ifbr_tagname[0])
		printf(" tag %s", r->ifbr_tagname);

	printf("\n");
}

/*
 * Parse a rule definition and send it upwards.
 *
 * Syntax:
 *	{block|pass} {in|out|in/out} on {ifs} [src {mac}] [dst {mac}]
 */
int
bridge_rule(int s, char *brdg, int targc, char **targv, int ln)
{
	char **argv = targv;
	int argc = targc;
	struct ifbrlreq rule;
	struct ether_addr *ea, *dea;

	if (argc == 0) {
		fprintf(stderr, "invalid rule\n");
		return (EX_USAGE);
	}
	rule.ifbr_tagname[0] = 0;
	rule.ifbr_flags = 0;
	rule.ifbr_action = 0;
	strlcpy(rule.ifbr_name, brdg, sizeof(rule.ifbr_name));

	if (strcmp(argv[0], "block") == 0)
		rule.ifbr_action = BRL_ACTION_BLOCK;
	else if (strcmp(argv[0], "pass") == 0)
		rule.ifbr_action = BRL_ACTION_PASS;
	else
		goto bad_rule;
	argc--;	argv++;

	if (argc == 0) {
		bridge_badrule(targc, targv, ln);
		return (EX_USAGE);
	}
	if (strcmp(argv[0], "in") == 0)
		rule.ifbr_flags |= BRL_FLAG_IN;
	else if (strcmp(argv[0], "out") == 0)
		rule.ifbr_flags |= BRL_FLAG_OUT;
	else if (strcmp(argv[0], "in/out") == 0)
		rule.ifbr_flags |= BRL_FLAG_IN | BRL_FLAG_OUT;
	else
		goto bad_rule;
	argc--; argv++;

	if (argc == 0 || strcmp(argv[0], "on"))
		goto bad_rule;
	argc--; argv++;

	if (argc == 0)
		goto bad_rule;
	strlcpy(rule.ifbr_ifsname, argv[0], sizeof(rule.ifbr_ifsname));
	argc--; argv++;

	while (argc) {
		if (strcmp(argv[0], "dst") == 0) {
			if (rule.ifbr_flags & BRL_FLAG_DSTVALID)
				goto bad_rule;
			rule.ifbr_flags |= BRL_FLAG_DSTVALID;
			dea = &rule.ifbr_dst;
		} else if (strcmp(argv[0], "src") == 0) {
			if (rule.ifbr_flags & BRL_FLAG_SRCVALID)
				goto bad_rule;
			rule.ifbr_flags |= BRL_FLAG_SRCVALID;
			dea = &rule.ifbr_src;
		} else if (strcmp(argv[0], "tag") == 0) {
			if (argc < 2) {
				fprintf(stderr, "missing tag name\n");
				goto bad_rule;
			}
			if (rule.ifbr_tagname[0]) {
				fprintf(stderr, "tag already defined\n");
				goto bad_rule;
			}
			if (strlcpy(rule.ifbr_tagname, argv[1],
			    PF_TAG_NAME_SIZE) > PF_TAG_NAME_SIZE) {
				fprintf(stderr, "tag name too long\n");
				goto bad_rule;
			}
			dea = NULL;
		} else
			goto bad_rule;

		argc--; argv++;

		if (argc == 0)
			goto bad_rule;
		if (dea != NULL) {
			ea = ether_aton(argv[0]);
			if (ea == NULL) {
				warnx("Invalid address: %s", argv[0]);
				return (EX_USAGE);
			}
			bcopy(ea, dea, sizeof(*dea));
		}
		argc--; argv++;
	}

	if (ioctl(s, SIOCBRDGARL, &rule) < 0) {
		warn("%s", brdg);
		return (EX_IOERR);
	}
	return (0);

bad_rule:
	bridge_badrule(targc, targv, ln);
	return (EX_USAGE);
}

#define MAXRULEWORDS 8

int
bridge_rulefile(int s, char *brdg, char *fname)
{
	FILE *f;
	char *str, *argv[MAXRULEWORDS], buf[1024], xbuf[1024];
	int ln = 1, argc = 0, err = 0, xerr;

	f = fopen(fname, "r");
	if (f == NULL) {
		warn("%s", fname);
		return (EX_IOERR);
	}

	while (1) {
		fgets(buf, sizeof(buf), f);
		if (feof(f))
			break;
		ln++;
		if (buf[0] == '#' || buf[0] == '\n')
			continue;

		argc = 0;
		str = strtok(buf, "\n\t\r ");
		strlcpy(xbuf, buf, sizeof(xbuf));
		while (str != NULL) {
			argv[argc++] = str;
			if (argc > MAXRULEWORDS) {
				fprintf(stderr, "invalid rule: %d: %s\n",
				    ln, xbuf);
				break;
			}
			str = strtok(NULL, "\n\t\r ");
		}

		if (argc > MAXRULEWORDS)
			continue;

		xerr = bridge_rule(s, brdg, argc, argv, ln);
		if (xerr)
			err = xerr;
	}
	fclose(f);
	return (err);
}

void
bridge_badrule(int argc, char *argv[], int ln)
{
	int i;

	fprintf(stderr, "invalid rule: ");
	if (ln != -1)
		fprintf(stderr, "%d: ", ln);
	for (i = 0; i < argc; i++) {
		fprintf(stderr, "%s ", argv[i]);
	}
	fprintf(stderr, "\n");
}

/*
 * Print a value ala the %b format of the kernel's printf
 * (borrowed from ifconfig.c)
 */
void
printb(char *s, unsigned short v, char *bits)
{
	int i, any = 0;
	char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v);
	else
		printf("%s=%x", s, v);
	bits++;
	if (bits) {
		putchar('<');
		while ((i = *bits++)) {
			if (v & (1 << (i-1))) {
				if (any)
					putchar(',');
				any = 1;
				for (; (c = *bits) > 32; bits++)
					putchar(c);
			} else
				for (; *bits > 32; bits++)
					;
		}
		putchar('>');
	}
}
