/*	$OpenBSD: device.c,v 1.3 1999/03/27 14:31:21 maja Exp $ */

/*
 * Copyright (c) 1993-95 Mats O Jansson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mats O Jansson.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LINT
static char rcsid[] = "$OpenBSD: device.c,v 1.3 1999/03/27 14:31:21 maja Exp $";
#endif

#include "os.h"
#include "common/common.h"
#include "common/mopdef.h"

struct	if_info *iflist;		/* Interface List		*/

void mopReadDL();
void mopReadRC();
#ifdef NO__P
int  mopOpenDL(/* struct if_info *, int */);
int  mopOpenRC(/* struct if_info *, int */);
#else
int  mopOpenDL(struct if_info *, int);
int  mopOpenRC(struct if_info *, int);
#endif
int pfTrans();
int pfInit();
int pfWrite();

#ifdef	DEV_NEW_CONF
/*
 * Return ethernet adress for interface
 */

void
deviceEthAddr(ifname, eaddr)
	char *ifname;
        u_char *eaddr;
{
	char inbuf[8192];
	struct ifconf ifc;
	struct ifreq *ifr;
	struct sockaddr_dl *sdl;
	int fd;
	int i, len;

	/* We cannot use SIOCGIFADDR on the BPF descriptor.
	   We must instead get all the interfaces with SIOCGIFCONF
	   and find the right one.  */

	/* Use datagram socket to get Ethernet address. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "deviceEthAddr: socket: %m");
		exit(1);
	}

	ifc.ifc_len = sizeof(inbuf);
	ifc.ifc_buf = inbuf;
	if (ioctl(fd, SIOCGIFCONF, (caddr_t)&ifc) < 0 ||
	    ifc.ifc_len < sizeof(struct ifreq)) {
		syslog(LOG_ERR, "deviceEthAddr: SIOGIFCONF: %m");
		exit(1);
	}
	ifr = ifc.ifc_req;
	for (i = 0; i < ifc.ifc_len;
	     i += len, ifr = (struct ifreq *)((caddr_t)ifr + len)) {
		len = sizeof(ifr->ifr_name) + ifr->ifr_addr.sa_len;
		sdl = (struct sockaddr_dl *)&ifr->ifr_addr;
		if (sdl->sdl_family != AF_LINK || sdl->sdl_type != IFT_ETHER ||
		    sdl->sdl_alen != 6)
			continue;
		if (!strncmp(ifr->ifr_name, ifname, sizeof(ifr->ifr_name))) {
			bcopy((caddr_t)LLADDR(sdl), (caddr_t)eaddr, 6);
			return;
		}
	}

	syslog(LOG_ERR, "deviceEthAddr: Never saw interface `%s'!", ifname);
	exit(1);
}
#endif	/* DEV_NEW_CONF */

void
deviceOpen(ifname, proto, trans)
	char	*ifname;
	u_short	 proto;
	int	 trans;
{
	struct if_info *p, tmp;

	strcpy(tmp.if_name,ifname);
	tmp.iopen   = pfInit;
	
	switch (proto) {
	case MOP_K_PROTO_RC:
		tmp.read = mopReadRC;
		tmp.fd   = mopOpenRC(&tmp, trans);
		break;
	case MOP_K_PROTO_DL:
		tmp.read = mopReadDL;
		tmp.fd   = mopOpenDL(&tmp, trans);
		break;
	default:
		break;
	}
	
	if (tmp.fd != -1) {
		
		p = (struct if_info *)malloc(sizeof(*p));
		if (p == 0) {
		syslog(LOG_ERR, "deviceOpen: malloc: %m");
		exit(1);
		}
	
		p->next = iflist;
		iflist = p;

		strcpy(p->if_name,tmp.if_name);
		p->iopen   = tmp.iopen;
		p->write   = pfWrite;
		p->read    = tmp.read;
		bzero((char *)p->eaddr,sizeof(p->eaddr));
		p->fd      = tmp.fd;

#ifdef	DEV_NEW_CONF
		deviceEthAddr(p->if_name,&p->eaddr[0]);
#else
		p->eaddr[0]= tmp.eaddr[0];
		p->eaddr[1]= tmp.eaddr[1];
		p->eaddr[2]= tmp.eaddr[2];
		p->eaddr[3]= tmp.eaddr[3];
		p->eaddr[4]= tmp.eaddr[4];
		p->eaddr[5]= tmp.eaddr[5];
#endif	/* DEV_NEW_CONF */
	
#ifdef LINUX2_PF
		{
			int s;

			s = socket(AF_INET,SOCK_DGRAM,0);
			pfEthAddr(s,p->if_name,&p->eaddr[0]);
			(void) close(s);
			
		}	
#endif
	}
}

void
deviceInitOne(ifname)
	char	*ifname;
{
	char	interface[IFNAME_SIZE];
	struct if_info *p;
	int	trans;
#ifdef _AIX
	char	dev[IFNAME_SIZE];
	int	unit,j;
	
	unit = 0;
	for (j = 0; j < strlen(ifname); j++) {
		if (isalpha(ifname[j])) {
			dev[j] = ifname[j];
		} else {
			if (isdigit(ifname[j])) {
				unit = unit*10 + ifname[j] - '0';
				dev[j] = '\0';
			}
		}
	}
	
	if ((strlen(dev) == 2) &&
	    (dev[0] == 'e') &&
	    ((dev[1] == 'n') || (dev[1] == 't'))) {
		sprintf(interface,"ent%d\0",unit);
	} else {
		sprintf(interface,"%s%d\0",dev,unit);
	}
#else
	sprintf(interface,"%s",ifname);
#endif /* _AIX */

	/* Ok, init it just once */
	
	p = iflist;
	for (p = iflist; p; p = p->next)  {
		if (strcmp(p->if_name,interface) == 0) {
			return;
		}
	}

	syslog(LOG_INFO, "Initialized %s", interface);

	/* Ok, get transport information */
	
	trans = pfTrans(interface);

#ifndef NORC
	/* Start with MOP Remote Console */

	switch (trans) {
	case TRANS_ETHER:
		deviceOpen(interface,MOP_K_PROTO_RC,TRANS_ETHER);
		break;
	case TRANS_8023:
		deviceOpen(interface,MOP_K_PROTO_RC,TRANS_8023);
		break;
	case TRANS_ETHER+TRANS_8023:
		deviceOpen(interface,MOP_K_PROTO_RC,TRANS_ETHER);
		deviceOpen(interface,MOP_K_PROTO_RC,TRANS_8023);
		break;
	case TRANS_ETHER+TRANS_8023+TRANS_AND:
		deviceOpen(interface,MOP_K_PROTO_RC,TRANS_ETHER+TRANS_8023);
		break;
	}
#endif

#ifndef NODL
	/* and next MOP Dump/Load */

	switch (trans) {
	case TRANS_ETHER:
		deviceOpen(interface,MOP_K_PROTO_DL,TRANS_ETHER);
		break;
	case TRANS_8023:
		deviceOpen(interface,MOP_K_PROTO_DL,TRANS_8023);
		break;
	case TRANS_ETHER+TRANS_8023:
		deviceOpen(interface,MOP_K_PROTO_DL,TRANS_ETHER);
		deviceOpen(interface,MOP_K_PROTO_DL,TRANS_8023);
		break;
	case TRANS_ETHER+TRANS_8023+TRANS_AND:
		deviceOpen(interface,MOP_K_PROTO_DL,TRANS_ETHER+TRANS_8023);
		break;
	}
#endif

}

/*
 * Initialize all "candidate" interfaces that are in the system
 * configuration list.  A "candidate" is up, not loopback and not
 * point to point.
 */
void
deviceInitAll()
{
#ifdef	DEV_NEW_CONF
	char inbuf[8192];
	struct ifconf ifc;
	struct ifreq *ifr;
	struct sockaddr_dl *sdl;
	int fd;
	int i, len;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "deviceInitAll: socket: %m");
		exit(1);
	}

	ifc.ifc_len = sizeof(inbuf);
	ifc.ifc_buf = inbuf;
	if (ioctl(fd, SIOCGIFCONF, (caddr_t)&ifc) < 0 ||
	    ifc.ifc_len < sizeof(struct ifreq)) {
		syslog(LOG_ERR, "deviceInitAll: SIOCGIFCONF: %m");
		exit(1);
	}
	ifr = ifc.ifc_req;
	for (i = 0; i < ifc.ifc_len;
	     i += len, ifr = (struct ifreq *)((caddr_t)ifr + len)) {
		len = sizeof(ifr->ifr_name) + ifr->ifr_addr.sa_len;
		sdl = (struct sockaddr_dl *)&ifr->ifr_addr;
		if (sdl->sdl_family != AF_LINK || sdl->sdl_type != IFT_ETHER ||
		    sdl->sdl_alen != 6)
			continue;
		if (ioctl(fd, SIOCGIFFLAGS, (caddr_t)ifr) < 0) {
			syslog(LOG_ERR, "deviceInitAll: SIOCGIFFLAGS: %m");
			continue;
		}
		if ((ifr->ifr_flags &
		    (IFF_UP | IFF_LOOPBACK | IFF_POINTOPOINT)) != IFF_UP)
			continue;
		deviceInitOne(ifr->ifr_name);
	}
	(void) close(fd);
#else
	int fd;
	int n;
	struct ifreq ibuf[8], *ifrp;
	struct ifconf ifc;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "deviceInitAll: old socket: %m");
		exit(1);
	}
	ifc.ifc_len = sizeof ibuf;
	ifc.ifc_buf = (caddr_t)ibuf;
	if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0 ||
	    ifc.ifc_len < sizeof(struct ifreq)) {
		syslog(LOG_ERR, "deviceInitAll: old SIOCGIFCONF: %m");
		exit(1);
	}
	ifrp = ibuf;
	n = ifc.ifc_len / sizeof(*ifrp);
	for (; --n >= 0; ++ifrp) {
		if (ioctl(fd, SIOCGIFFLAGS, (char *)ifrp) < 0) {
			continue;
		}
		if (/*(ifrp->ifr_flags & IFF_UP) == 0 ||*/
		    ifrp->ifr_flags & IFF_LOOPBACK ||
		    ifrp->ifr_flags & IFF_POINTOPOINT)
			continue;
		deviceInitOne(ifrp->ifr_name);
	}
	
	(void) close(fd);
#endif /* DEV_NEW_CONF */
}
