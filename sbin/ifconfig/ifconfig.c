/*	$OpenBSD: ifconfig.c,v 1.232 2010/05/06 12:58:40 claudio Exp $	*/
/*	$NetBSD: ifconfig.c,v 1.40 1997/10/01 02:19:43 enami Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1997, 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/nd6.h>
#include <arpa/inet.h>
#include <netinet/ip_ipsp.h>
#include <netinet/if_ether.h>
#include <net/if_enc.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <net/pfvar.h>
#include <net/if_pfsync.h>
#include <net/if_pflow.h>
#include <net/if_pppoe.h>
#include <net/if_trunk.h>
#include <net/if_sppp.h>
#include <net/ppp_defs.h>

#include <netatalk/at.h>

#include <netinet/ip_carp.h>

#include <netdb.h>

#include <net/if_vlan_var.h>

#include <netmpls/mpls.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>

#include "brconfig.h"

struct	ifreq		ifr, ridreq;
struct	in_aliasreq	in_addreq;
#ifdef INET6
struct	in6_ifreq	ifr6;
struct	in6_ifreq	in6_ridreq;
struct	in6_aliasreq	in6_addreq;
#endif /* INET6 */
struct	sockaddr_in	netmask;

#ifndef SMALL
struct	ifaliasreq	addreq;
struct  netrange	at_nr;		/* AppleTalk net range */
#endif /* SMALL */

char	name[IFNAMSIZ];
int	flags, setaddr, setipdst, doalias;
u_long	metric, mtu;
int	rdomainid;
int	clearaddr, s;
int	newaddr = 0;
int	af = AF_INET;
int	explicit_prefix = 0;
#ifdef INET6
int	Lflag = 1;
#endif /* INET6 */

int	showmediaflag;
int	shownet80211chans;
int	shownet80211nodes;

void	notealias(const char *, int);
void	notrailers(const char *, int);
void	setifgroup(const char *, int);
void	unsetifgroup(const char *, int);
void	setifaddr(const char *, int);
void	setifrtlabel(const char *, int);
void	setiflladdr(const char *, int);
void	setifdstaddr(const char *, int);
void	setifflags(const char *, int);
void	setifxflags(const char *, int);
void	setifbroadaddr(const char *, int);
void	setifdesc(const char *, int);
void	unsetifdesc(const char *, int);
void	setifipdst(const char *, int);
void	setifmetric(const char *, int);
void	setifmtu(const char *, int);
void	setifnwid(const char *, int);
void	setifbssid(const char *, int);
void	setifnwkey(const char *, int);
void	setifwpa(const char *, int);
void	setifwpaprotos(const char *, int);
void	setifwpaakms(const char *, int);
void	setifwpaciphers(const char *, int);
void	setifwpagroupcipher(const char *, int);
void	setifwpapsk(const char *, int);
void	setifchan(const char *, int);
void	setifscan(const char *, int);
void	setiftxpower(const char *, int);
void	setifpowersave(const char *, int);
void	setifnwflag(const char *, int);
void	unsetifnwflag(const char *, int);
void	setifnetmask(const char *, int);
void	setifprefixlen(const char *, int);
void	setatrange(const char *, int);
void	setatphase(const char *, int);
void	settunnel(const char *, const char *);
void	deletetunnel(const char *, int);
void	settunnelinst(const char *, int);
#ifdef INET6
void	setia6flags(const char *, int);
void	setia6pltime(const char *, int);
void	setia6vltime(const char *, int);
void	setia6lifetime(const char *, const char *);
void	setia6eui64(const char *, int);
#endif /* INET6 */
void	checkatrange(struct sockaddr_at *);
void	setmedia(const char *, int);
void	setmediaopt(const char *, int);
void	setmediamode(const char *, int);
void	clone_create(const char *, int);
void	clone_destroy(const char *, int);
void	unsetmediaopt(const char *, int);
void	setmediainst(const char *, int);
void	settimeslot(const char *, int);
void	timeslot_status(void);
void	setmpelabel(const char *, int);
void	setvlantag(const char *, int);
void	setvlanprio(const char *, int);
void	setvlandev(const char *, int);
void	unsetvlandev(const char *, int);
void	mpe_status(void);
void	vlan_status(void);
void	getifgroups(void);
void	carp_status(void);
void	setcarp_advbase(const char *,int);
void	setcarp_advskew(const char *, int);
void	setcarppeer(const char *, int);
void	unsetcarppeer(const char *, int);
void	setcarp_passwd(const char *, int);
void	setcarp_vhid(const char *, int);
void	setcarp_state(const char *, int);
void	setcarpdev(const char *, int);
void	unsetcarpdev(const char *, int);
void	setcarp_nodes(const char *, int);
void	setcarp_balancing(const char *, int);
void	setpfsync_syncdev(const char *, int);
void	setpfsync_maxupd(const char *, int);
void	unsetpfsync_syncdev(const char *, int);
void	setpfsync_syncpeer(const char *, int);
void	unsetpfsync_syncpeer(const char *, int);
void	setpfsync_defer(const char *, int);
void	pfsync_status(void);
void	setpppoe_dev(const char *,int);
void	setpppoe_svc(const char *,int);
void	setpppoe_ac(const char *,int);
void	pppoe_status(void);
void	setspppproto(const char *, int);
void	setspppname(const char *, int);
void	setspppkey(const char *, int);
void	setsppppeerproto(const char *, int);
void	setsppppeername(const char *, int);
void	setsppppeerkey(const char *, int);
void	setsppppeerflag(const char *, int);
void	unsetsppppeerflag(const char *, int);
void	spppinfo(struct spppreq *);
void	sppp_status(void);
void	sppp_printproto(const char *, struct sauthreq *);
void	settrunkport(const char *, int);
void	unsettrunkport(const char *, int);
void	settrunkproto(const char *, int);
void	trunk_status(void);
void	setifpriority(const char *, int);
void	setinstance(const char *, int);
int	main(int, char *[]);
int	prefix(void *val, int);

#ifndef SMALL
void	pflow_status(void);
void	setpflow_sender(const char *, int);
void	unsetpflow_sender(const char *, int);
void	setpflow_receiver(const char *, int);
void	unsetpflow_receiver(const char *, int);
#endif

/*
 * Media stuff.  Whenever a media command is first performed, the
 * currently select media is grabbed for this interface.  If `media'
 * is given, the current media word is modified.  `mediaopt' commands
 * only modify the set and clear words.  They then operate on the
 * current media word later.
 */
int	media_current;
int	mediaopt_set;
int	mediaopt_clear;

int	actions;			/* Actions performed */

#define	A_MEDIA		0x0001		/* media command */
#define	A_MEDIAOPTSET	0x0002		/* mediaopt command */
#define	A_MEDIAOPTCLR	0x0004		/* -mediaopt command */
#define	A_MEDIAOPT	(A_MEDIAOPTSET|A_MEDIAOPTCLR)
#define	A_MEDIAINST	0x0008		/* instance or inst command */
#define	A_MEDIAMODE	0x0010		/* mode command */
#define A_SILENT	0x8000000	/* doing operation, do not print */

#define	NEXTARG0	0xffffff
#define NEXTARG		0xfffffe
#define	NEXTARG2	0xfffffd

const struct	cmd {
	char	*c_name;
	int	c_parameter;		/* NEXTARG means next argv */
	int	c_action;		/* defered action */
	void	(*c_func)(const char *, int);
	void	(*c_func2)(const char *, const char *);
} cmds[] = {
	{ "up",		IFF_UP,		0,		setifflags } ,
	{ "down",	-IFF_UP,	0,		setifflags },
	{ "trailers",	-1,		0,		notrailers },
	{ "-trailers",	1,		0,		notrailers },
	{ "arp",	-IFF_NOARP,	0,		setifflags },
	{ "-arp",	IFF_NOARP,	0,		setifflags },
	{ "debug",	IFF_DEBUG,	0,		setifflags },
	{ "-debug",	-IFF_DEBUG,	0,		setifflags },
	{ "alias",	IFF_UP,		0,		notealias },
	{ "-alias",	-IFF_UP,	0,		notealias },
	{ "delete",	-IFF_UP,	0,		notealias },
#ifdef notdef
#define	EN_SWABIPS	0x1000
	{ "swabips",	EN_SWABIPS,	0,		setifflags },
	{ "-swabips",	-EN_SWABIPS,	0,		setifflags },
#endif /* notdef */
	{ "group",	NEXTARG,	0,		setifgroup },
	{ "-group",	NEXTARG,	0,		unsetifgroup },
	{ "netmask",	NEXTARG,	0,		setifnetmask },
	{ "metric",	NEXTARG,	0,		setifmetric },
	{ "mtu",	NEXTARG,	0,		setifmtu },
	{ "nwid",	NEXTARG,	0,		setifnwid },
	{ "-nwid",	-1,		0,		setifnwid },
	{ "bssid",	NEXTARG,	0,		setifbssid },
	{ "-bssid",	-1,		0,		setifbssid },
	{ "nwkey",	NEXTARG,	0,		setifnwkey },
	{ "-nwkey",	-1,		0,		setifnwkey },
	{ "wpa",	1,		0,		setifwpa },
	{ "-wpa",	0,		0,		setifwpa },
	{ "wpaakms",	NEXTARG,	0,		setifwpaakms },
	{ "wpaciphers",	NEXTARG,	0,		setifwpaciphers },
	{ "wpagroupcipher", NEXTARG,	0,		setifwpagroupcipher },
	{ "wpaprotos",	NEXTARG,	0,		setifwpaprotos },
	{ "wpapsk",	NEXTARG,	0,		setifwpapsk },
	{ "-wpapsk",	-1,		0,		setifwpapsk },
	{ "chan",	NEXTARG0,	0,		setifchan },
	{ "-chan",	-1,		0,		setifchan },
	{ "scan",	NEXTARG0,	0,		setifscan },
	{ "powersave",	NEXTARG0,	0,		setifpowersave },
	{ "-powersave",	-1,		0,		setifpowersave },
	{ "broadcast",	NEXTARG,	0,		setifbroadaddr },
	{ "ipdst",	NEXTARG,	0,		setifipdst },
	{ "prefixlen",  NEXTARG,	0,		setifprefixlen},
	{ "priority",	NEXTARG,	0,		setifpriority },
	{ "vlan",	NEXTARG,	0,		setvlantag },
	{ "vlanprio",	NEXTARG,	0,		setvlanprio },
	{ "vlandev",	NEXTARG,	0,		setvlandev },
	{ "-vlandev",	1,		0,		unsetvlandev },
#ifdef INET6
	{ "anycast",	IN6_IFF_ANYCAST,	0,	setia6flags },
	{ "-anycast",	-IN6_IFF_ANYCAST,	0,	setia6flags },
	{ "tentative",	IN6_IFF_TENTATIVE,	0,	setia6flags },
	{ "-tentative",	-IN6_IFF_TENTATIVE,	0,	setia6flags },
	{ "pltime",	NEXTARG,	0,		setia6pltime },
	{ "vltime",	NEXTARG,	0,		setia6vltime },
	{ "eui64",	0,		0,		setia6eui64 },
	{ "autoconfprivacy",	IFXF_INET6_PRIVACY,	0,	setifxflags },
	{ "-autoconfprivacy",	-IFXF_INET6_PRIVACY,	0,	setifxflags },
#endif /*INET6*/
#ifndef SMALL
	{ "rtlabel",	NEXTARG,	0,		setifrtlabel },
	{ "-rtlabel",	-1,		0,		setifrtlabel },
	{ "range",	NEXTARG,	0,		setatrange },
	{ "phase",	NEXTARG,	0,		setatphase },
	{ "mplslabel",	NEXTARG,	0,		setmpelabel },
	{ "advbase",	NEXTARG,	0,		setcarp_advbase },
	{ "advskew",	NEXTARG,	0,		setcarp_advskew },
	{ "carppeer",	NEXTARG,	0,		setcarppeer },
	{ "-carppeer",	1,		0,		unsetcarppeer },
	{ "pass",	NEXTARG,	0,		setcarp_passwd },
	{ "vhid",	NEXTARG,	0,		setcarp_vhid },
	{ "state",	NEXTARG,	0,		setcarp_state },
	{ "carpdev",	NEXTARG,	0,		setcarpdev },
	{ "carpnodes",	NEXTARG,	0,		setcarp_nodes },
	{ "balancing",	NEXTARG,	0,		setcarp_balancing },
	{ "-carpdev",	1,		0,		unsetcarpdev },
	{ "syncdev",	NEXTARG,	0,		setpfsync_syncdev },
	{ "-syncdev",	1,		0,		unsetpfsync_syncdev },
	{ "syncif",	NEXTARG,	0,		setpfsync_syncdev },
	{ "-syncif",	1,		0,		unsetpfsync_syncdev },
	{ "syncpeer",	NEXTARG,	0,		setpfsync_syncpeer },
	{ "-syncpeer",	1,		0,		unsetpfsync_syncpeer },
	{ "maxupd",	NEXTARG,	0,		setpfsync_maxupd },
	{ "defer",	1,		0,		setpfsync_defer },
	{ "-defer",	0,		0,		setpfsync_defer },
	/* giftunnel is for backward compat */
	{ "giftunnel",  NEXTARG2,	0,		NULL, settunnel } ,
	{ "tunnel",	NEXTARG2,	0,		NULL, settunnel } ,
	{ "deletetunnel",  0,		0,		deletetunnel } ,
	{ "tunneldomain", NEXTARG,	0,		settunnelinst } ,
	{ "pppoedev",	NEXTARG,	0,		setpppoe_dev },
	{ "pppoesvc",	NEXTARG,	0,		setpppoe_svc },
	{ "-pppoesvc",	1,		0,		setpppoe_svc },
	{ "pppoeac",	NEXTARG,	0,		setpppoe_ac },
	{ "-pppoeac",	1,		0,		setpppoe_ac },
	{ "timeslot",	NEXTARG,	0,		settimeslot },
	{ "txpower",	NEXTARG,	0,		setiftxpower },
	{ "-txpower",	1,		0,		setiftxpower },
	{ "trunkport",	NEXTARG,	0,		settrunkport },
	{ "-trunkport",	NEXTARG,	0,		unsettrunkport },
	{ "trunkproto",	NEXTARG,	0,		settrunkproto },
	{ "authproto",	NEXTARG,	0,		setspppproto },
	{ "authname",	NEXTARG,	0,		setspppname },
	{ "authkey",	NEXTARG,	0,		setspppkey },
	{ "peerproto",	NEXTARG,	0,		setsppppeerproto },
	{ "peername",	NEXTARG,	0,		setsppppeername },
	{ "peerkey",	NEXTARG,	0,		setsppppeerkey },
	{ "peerflag",	NEXTARG,	0,		setsppppeerflag },
	{ "-peerflag",	NEXTARG,	0,		unsetsppppeerflag },
	{ "nwflag",	NEXTARG,	0,		setifnwflag },
	{ "-nwflag",	NEXTARG,	0,		unsetifnwflag },
	{ "rdomain",	NEXTARG,	0,		setinstance },
	{ "flowsrc",	NEXTARG,	0,		setpflow_sender },
	{ "-flowsrc",	1,		0,		unsetpflow_sender },
	{ "flowdst", 	NEXTARG,	0,		setpflow_receiver },
	{ "-flowdst", 1,		0,		unsetpflow_receiver },
	{ "-inet6",	IFXF_NOINET6,	0,		setifxflags } ,
	{ "add",	NEXTARG,	0,		bridge_add },
	{ "del",	NEXTARG,	0,		bridge_delete },
	{ "addspan",	NEXTARG,	0,		bridge_addspan },
	{ "delspan",	NEXTARG, 	0,		bridge_delspan },
	{ "discover",	NEXTARG,	0,		setdiscover },
	{ "-discover",	NEXTARG,	0,		unsetdiscover },
	{ "blocknonip", NEXTARG,	0,		setblocknonip },
	{ "-blocknonip",NEXTARG,	0,		unsetblocknonip },
	{ "learn",	NEXTARG,	0,		setlearn },
	{ "-learn",	NEXTARG,	0,		unsetlearn },
	{ "stp",	NEXTARG,	0,		setstp },
	{ "-stp",	NEXTARG,	0,		unsetstp },
	{ "edge",	NEXTARG,	0,		setedge },
	{ "-edge",	NEXTARG,	0,		unsetedge },
	{ "autoedge",	NEXTARG,	0,		setautoedge },
	{ "-autoedge",	NEXTARG,	0,		unsetautoedge },
	{ "ptp",	NEXTARG,	0,		setptp },
	{ "-ptp",	NEXTARG,	0,		unsetptp },
	{ "autoptp",	NEXTARG,	0,		setautoptp },
	{ "-autoptp",	NEXTARG,	0,		unsetautoptp },
	{ "flush",	0,		0,		bridge_flush },
	{ "flushall",	0,		0,		bridge_flushall },
	{ "static",	NEXTARG2,	0,		NULL, bridge_addaddr },
	{ "deladdr",	NEXTARG,	0,		bridge_deladdr },
	{ "maxaddr",	NEXTARG,	0,		bridge_maxaddr },
	{ "addr",	0,		0,		bridge_addrs },
	{ "hellotime",	NEXTARG,	0,		bridge_hellotime },
	{ "fwddelay",	NEXTARG,	0,		bridge_fwddelay },
	{ "maxage",	NEXTARG,	0,		bridge_maxage },
	{ "proto",	NEXTARG,	0,		bridge_proto },
	{ "ifpriority",	NEXTARG2,	0,		NULL, bridge_ifprio },
	{ "ifcost",	NEXTARG2,	0,		NULL, bridge_ifcost },
	{ "-ifcost",	NEXTARG,	0,		bridge_noifcost },
	{ "timeout",	NEXTARG,	0,		bridge_timeout },
	{ "holdcnt",	NEXTARG,	0,		bridge_holdcnt },
	{ "spanpriority", NEXTARG,	0,		bridge_priority },
#if 0
	/* XXX `rule` special-cased below */
	{ "rule",	0,		0,		bridge_rule },
#endif
	{ "rules",	NEXTARG,	0,		bridge_rules },
	{ "rulefile",	NEXTARG,	0,		bridge_rulefile },
	{ "flushrule",	NEXTARG,	0,		bridge_flushrule },
#endif /* SMALL */
#if 0
	/* XXX `create' special-cased below */
	{ "create",	0,		0,		clone_create } ,
#endif
	{ "destroy",	0,		0,		clone_destroy } ,
	{ "link0",	IFF_LINK0,	0,		setifflags } ,
	{ "-link0",	-IFF_LINK0,	0,		setifflags } ,
	{ "link1",	IFF_LINK1,	0,		setifflags } ,
	{ "-link1",	-IFF_LINK1,	0,		setifflags } ,
	{ "link2",	IFF_LINK2,	0,		setifflags } ,
	{ "-link2",	-IFF_LINK2,	0,		setifflags } ,
	{ "media",	NEXTARG0,	A_MEDIA,	setmedia },
	{ "mediaopt",	NEXTARG,	A_MEDIAOPTSET,	setmediaopt },
	{ "-mediaopt",	NEXTARG,	A_MEDIAOPTCLR,	unsetmediaopt },
	{ "mode",	NEXTARG,	A_MEDIAMODE,	setmediamode },
	{ "instance",	NEXTARG,	A_MEDIAINST,	setmediainst },
	{ "inst",	NEXTARG,	A_MEDIAINST,	setmediainst },
	{ "lladdr",	NEXTARG,	0,		setiflladdr },
	{ "description", NEXTARG,	0,		setifdesc },
	{ "descr",	NEXTARG,	0,		setifdesc },
	{ "-description", 1,		0,		unsetifdesc },
	{ "-descr",	1,		0,		unsetifdesc },
	{ NULL, /*src*/	0,		0,		setifaddr },
	{ NULL, /*dst*/	0,		0,		setifdstaddr },
	{ NULL, /*illegal*/0,		0,		NULL },
};

int	getinfo(struct ifreq *, int);
void	getsock(int);
int	printgroup(char *, int);
void	printgroupattribs(char *);
void	setgroupattribs(char *, int, char *[]);
void	printif(char *, int);
void	printb(char *, unsigned short, char *);
void	printb_status(unsigned short, char *);
const char *get_linkstate(int, int);
void	status(int, struct sockaddr_dl *, int);
void	usage(int);
const char *get_string(const char *, const char *, u_int8_t *, int *);
void	print_string(const u_int8_t *, int);
char	*sec2str(time_t);
void	list_cloners(void);

const char *get_media_type_string(int);
const char *get_media_subtype_string(int);
int	get_media_mode(int, const char *);
int	get_media_subtype(int, const char *);
int	get_media_options(int, const char *);
int	lookup_media_word(const struct ifmedia_description *, int,
	    const char *);
void	print_media_word(int, int, int);
void	process_media_commands(void);
void	init_current_media(void);

unsigned long get_ts_map(int, int, int);

void	in_status(int);
void	in_getaddr(const char *, int);
void	in_getprefix(const char *, int);
#ifdef INET6
void	in6_fillscopeid(struct sockaddr_in6 *sin6);
void	in6_alias(struct in6_ifreq *);
void	in6_status(int);
void	in6_getaddr(const char *, int);
void	in6_getprefix(const char *, int);
#endif /* INET6 */
void    at_status(int);
void    at_getaddr(const char *, int);
void	ieee80211_status(void);
void	ieee80211_listchans(void);
void	ieee80211_listnodes(void);
void	ieee80211_printnode(struct ieee80211_nodereq *);

/* Known address families */
const struct afswtch {
	char *af_name;
	short af_af;
	void (*af_status)(int);
	void (*af_getaddr)(const char *, int);
	void (*af_getprefix)(const char *, int);
	u_long af_difaddr;
	u_long af_aifaddr;
	caddr_t af_ridreq;
	caddr_t af_addreq;
} afs[] = {
#define C(x) ((caddr_t) &x)
	{ "inet", AF_INET, in_status, in_getaddr, in_getprefix,
	    SIOCDIFADDR, SIOCAIFADDR, C(ridreq), C(in_addreq) },
#ifdef INET6
	{ "inet6", AF_INET6, in6_status, in6_getaddr, in6_getprefix,
	    SIOCDIFADDR_IN6, SIOCAIFADDR_IN6, C(in6_ridreq), C(in6_addreq) },
#endif /* INET6 */
#ifndef SMALL
	{ "atalk", AF_APPLETALK, at_status, at_getaddr, NULL,
	    SIOCDIFADDR, SIOCAIFADDR, C(addreq), C(addreq) },
#endif
	{ 0,	0,	    0,		0 }
};

const struct afswtch *afp;	/*the address family being set or asked about*/

int ifaliases = 0;
int aflag = 0;

int
main(int argc, char *argv[])
{
	const struct afswtch *rafp = NULL;
	int create = 0;
	int Cflag = 0;
	int gflag = 0;
	int i;
	int noprint = 0;

	/* If no args at all, print all interfaces.  */
	if (argc < 2) {
		aflag = 1;
		printif(NULL, 0);
		exit(0);
	}
	argc--, argv++;
	if (*argv[0] == '-') {
		int nomore = 0;

		for (i = 1; argv[0][i]; i++) {
			switch (argv[0][i]) {
			case 'a':
				aflag = 1;
				nomore = 1;
				break;
			case 'A':
				aflag = 1;
				ifaliases = 1;
				nomore = 1;
				break;
			case 'g':
				gflag = 1;
				break;
			case 'C':
				Cflag = 1;
				nomore = 1;
				break;
			default:
				usage(1);
				break;
			}
		}
		if (nomore == 0) {
			argc--, argv++;
			if (argc < 1)
				usage(1);
			if (strlcpy(name, *argv, sizeof(name)) >= IFNAMSIZ)
				errx(1, "interface name '%s' too long", *argv);
		}
	} else if (strlcpy(name, *argv, sizeof(name)) >= IFNAMSIZ)
		errx(1, "interface name '%s' too long", *argv);
	argc--, argv++;
	if (argc > 0) {
		for (afp = rafp = afs; rafp->af_name; rafp++)
			if (strcmp(rafp->af_name, *argv) == 0) {
				afp = rafp;
				argc--;
				argv++;
				break;
			}
		rafp = afp;
		af = ifr.ifr_addr.sa_family = rafp->af_af;
	}
	if (Cflag) {
		if (argc > 0 || aflag)
			usage(1);
		list_cloners();
		exit(0);
	}
	if (gflag) {
		if (argc == 0)
			printgroupattribs(name);
		else
			setgroupattribs(name, argc, argv);
		exit(0);
	}
	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

#ifdef INET6
	/* initialization */
	in6_addreq.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
	in6_addreq.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
#endif /* INET6 */
	/*
	 * NOTE:  We must special-case the `create' command right
	 * here as we would otherwise fail in getinfo().
	 */
	if (argc > 0 && strcmp(argv[0], "create") == 0) {
		clone_create(argv[0], 0);
		argc--, argv++;
		if (argc == 0)
			exit(0);
	}
	if (aflag == 0) {
		create = (argc > 0) && strcmp(argv[0], "destroy") != 0;
		(void)getinfo(&ifr, create);
	}
#ifdef INET6
	if (argc != 0 && af == AF_INET6)
		setifxflags("inet6", -IFXF_NOINET6);
#endif
	while (argc > 0) {
		const struct cmd *p;

		for (p = cmds; p->c_name; p++)
			if (strcmp(*argv, p->c_name) == 0)
				break;
#ifndef SMALL
		if (strcmp(*argv, "rule") == 0) {
			argc--, argv++;
			return bridge_rule(argc, argv, -1);
		}
#endif
		if (p->c_name == 0 && setaddr)
			for (i = setaddr; i > 0; i--) {
				p++;
				if (p->c_func == NULL)
					errx(1, "%s: bad value", *argv);
			}
		if (p->c_func || p->c_func2) {
			if (p->c_parameter == NEXTARG0) {
				const struct cmd *p0;
				int noarg = 1;

				if (argv[1]) {
					for (p0 = cmds; p0->c_name; p0++)
						if (strcmp(argv[1], p0->c_name) == 0) {
							noarg = 0;
							break;
						}
				} else
					noarg = 0;

				if (noarg == 0)
					(*p->c_func)(NULL, 0);
				else
					goto nextarg;
			} else if (p->c_parameter == NEXTARG) {
nextarg:
				if (argv[1] == NULL)
					errx(1, "'%s' requires argument",
					    p->c_name);
				(*p->c_func)(argv[1], 0);
				argc--, argv++;
				actions = actions | A_SILENT | p->c_action;
			} else if (p->c_parameter == NEXTARG2) {
				if ((argv[1] == NULL) ||
				    (argv[2] == NULL))
					errx(1, "'%s' requires 2 arguments",
					    p->c_name);
				(*p->c_func2)(argv[1], argv[2]);
				argc -= 2;
				argv += 2;
				actions = actions | A_SILENT | p->c_action;
			} else {
				(*p->c_func)(*argv, p->c_parameter);
				actions = actions | A_SILENT | p->c_action;
			}
		}
		argc--, argv++;
	}

	if (argc == 0 && actions == 0 && !noprint) {
		printif(ifr.ifr_name, aflag ? ifaliases : 1);
		exit(0);
	}

	/* Process any media commands that may have been issued. */
	process_media_commands();

	if (af == AF_INET6 && explicit_prefix == 0) {
		/*
		 * Aggregatable address architecture defines all prefixes
		 * are 64. So, it is convenient to set prefixlen to 64 if
		 * it is not specified.
		 */
		setifprefixlen("64", 0);
		/* in6_getprefix("64", MASK) if MASK is available here... */
	}

#ifndef SMALL
	if (af == AF_APPLETALK)
		checkatrange((struct sockaddr_at *) &addreq.ifra_addr);
#endif /* SMALL */

	if (clearaddr) {
		(void) strlcpy(rafp->af_ridreq, name, sizeof(ifr.ifr_name));
		if (ioctl(s, rafp->af_difaddr, rafp->af_ridreq) < 0) {
			if (errno == EADDRNOTAVAIL && (doalias >= 0)) {
				/* means no previous address for interface */
			} else
				err(1, "SIOCDIFADDR");
		}
	}
	if (newaddr) {
		(void) strlcpy(rafp->af_addreq, name, sizeof(ifr.ifr_name));
		if (ioctl(s, rafp->af_aifaddr, rafp->af_addreq) < 0)
			err(1, "SIOCAIFADDR");
	}
	exit(0);
}

void
getsock(int naf)
{
	static int oaf = -1;

	if (oaf == naf)
		return;
	if (oaf != -1)
		close(s);
	s = socket(naf, SOCK_DGRAM, 0);
	if (s < 0)
		oaf = -1;
	else
		oaf = naf;
}

int
getinfo(struct ifreq *ifr, int create)
{

	getsock(af);
	if (s < 0)
		err(1, "socket");
	if (!isdigit(name[strlen(name) - 1]))
		return (-1);	/* ignore groups here */
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)ifr) < 0) {
		int oerrno = errno;

		if (!create)
			return (-1);
		if (ioctl(s, SIOCIFCREATE, (caddr_t)ifr) < 0) {
			errno = oerrno;
			return (-1);
		}
		if (ioctl(s, SIOCGIFFLAGS, (caddr_t)ifr) < 0)
			return (-1);
	}
	flags = ifr->ifr_flags;
	if (ioctl(s, SIOCGIFMETRIC, (caddr_t)ifr) < 0)
		metric = 0;
	else
		metric = ifr->ifr_metric;
#ifdef SMALL
	if (ioctl(s, SIOCGIFMTU, (caddr_t)ifr) < 0)
#else
	if (is_bridge(name) || ioctl(s, SIOCGIFMTU, (caddr_t)ifr) < 0)
#endif
		mtu = 0;
	else
		mtu = ifr->ifr_mtu;
	if (ioctl(s, SIOCGIFRTABLEID, (caddr_t)ifr) < 0)
		rdomainid = 0;
	else
		rdomainid = ifr->ifr_rdomainid;
	return (0);
}

int
printgroup(char *groupname, int ifaliases)
{
	struct ifgroupreq	 ifgr;
	struct ifg_req		*ifg;
	int			 len, cnt = 0;

	getsock(AF_INET);
	bzero(&ifgr, sizeof(ifgr));
	strlcpy(ifgr.ifgr_name, groupname, sizeof(ifgr.ifgr_name));
	if (ioctl(s, SIOCGIFGMEMB, (caddr_t)&ifgr) == -1) {
		if (errno == EINVAL || errno == ENOTTY ||
		    errno == ENOENT)
			return (-1);
		else
			err(1, "SIOCGIFGMEMB");
	}

	len = ifgr.ifgr_len;
	if ((ifgr.ifgr_groups = calloc(1, len)) == NULL)
		err(1, "printgroup");
	if (ioctl(s, SIOCGIFGMEMB, (caddr_t)&ifgr) == -1)
		err(1, "SIOCGIFGMEMB");

	for (ifg = ifgr.ifgr_groups; ifg && len >= sizeof(struct ifg_req);
	    ifg++) {
		len -= sizeof(struct ifg_req);
		printif(ifg->ifgrq_member, ifaliases);
		cnt++;
	}
	free(ifgr.ifgr_groups);

	return (cnt);
}

void
printgroupattribs(char *groupname)
{
	struct ifgroupreq	 ifgr;

	getsock(AF_INET);
	bzero(&ifgr, sizeof(ifgr));
	strlcpy(ifgr.ifgr_name, groupname, sizeof(ifgr.ifgr_name));
	if (ioctl(s, SIOCGIFGATTR, (caddr_t)&ifgr) == -1)
		err(1, "SIOCGIFGATTR");

	printf("%s:", groupname);
	printf(" carp demote count %d", ifgr.ifgr_attrib.ifg_carp_demoted);
	printf("\n");
}

void
setgroupattribs(char *groupname, int argc, char *argv[])
{
	const char *errstr;
	char *p = argv[0];
	int neg = 1;

	struct ifgroupreq	 ifgr;

	getsock(AF_INET);
	bzero(&ifgr, sizeof(ifgr));
	strlcpy(ifgr.ifgr_name, groupname, sizeof(ifgr.ifgr_name));

	if (argc > 1) {
		neg = strtonum(argv[1], 0, 128, &errstr);
		if (errstr)
			errx(1, "invalid carp demotion: %s", errstr);
	}

	if (p[0] == '-') {
		neg = neg * -1;
		p++;
	}
	if (!strcmp(p, "carpdemote"))
		ifgr.ifgr_attrib.ifg_carp_demoted = neg;
	else
		usage(1);

	if (ioctl(s, SIOCSIFGATTR, (caddr_t)&ifgr) == -1)
		err(1, "SIOCSIFGATTR");
}

void
printif(char *ifname, int ifaliases)
{
	struct ifaddrs *ifap, *ifa;
	struct if_data *ifdata;
	const char *namep;
	char *oname = NULL;
	struct ifreq *ifrp;
	int count = 0, noinet = 1;
	size_t nlen = 0;

	if (aflag)
		ifname = NULL;
	if (ifname) {
		if ((oname = strdup(ifname)) == NULL)
			err(1, "strdup");
		nlen = strlen(oname);
		if (nlen && !isdigit(oname[nlen - 1]))	/* is it a group? */
			if (printgroup(oname, ifaliases) != -1)
				return;
	}

	if (getifaddrs(&ifap) != 0)
		err(1, "getifaddrs");

	namep = NULL;
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (oname) {
			if (nlen && isdigit(oname[nlen - 1])) {
				/* must have exact match */
				if (strcmp(oname, ifa->ifa_name) != 0)
					continue;
			} else {
				/* partial match OK if it ends w/ digit */
				if (strncmp(oname, ifa->ifa_name, nlen) != 0 ||
				    !isdigit(ifa->ifa_name[nlen]))
					continue;
			}
		}
#ifdef INET6
		/* quickhack: sizeof(ifr) < sizeof(ifr6) */
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			memset(&ifr6, 0, sizeof(ifr6));
			memcpy(&ifr6.ifr_addr, ifa->ifa_addr,
			    MIN(sizeof(ifr6.ifr_addr), ifa->ifa_addr->sa_len));
			ifrp = (struct ifreq *)&ifr6;
		} else
#endif
		{
			memset(&ifr, 0, sizeof(ifr));
			memcpy(&ifr.ifr_addr, ifa->ifa_addr,
			    MIN(sizeof(ifr.ifr_addr), ifa->ifa_addr->sa_len));
			ifrp = &ifr;
		}
		strlcpy(name, ifa->ifa_name, sizeof(name));
		strlcpy(ifrp->ifr_name, ifa->ifa_name, sizeof(ifrp->ifr_name));

		if (ifa->ifa_addr->sa_family == AF_LINK) {
			namep = ifa->ifa_name;
			if (getinfo(ifrp, 0) < 0)
				continue;
			ifdata = ifa->ifa_data;
			status(1, (struct sockaddr_dl *)ifa->ifa_addr,
			    ifdata->ifi_link_state);
			count++;
			noinet = 1;
			continue;
		}

		if (!namep || !strcmp(namep, ifa->ifa_name)) {
			const struct afswtch *p;

			if (ifa->ifa_addr->sa_family == AF_INET &&
			    ifaliases == 0 && noinet == 0)
				continue;
			if ((p = afp) != NULL) {
				if (ifa->ifa_addr->sa_family == p->af_af)
					(*p->af_status)(1);
			} else {
				for (p = afs; p->af_name; p++) {
					if (ifa->ifa_addr->sa_family ==
					    p->af_af)
						(*p->af_status)(0);
				}
			}
			count++;
			if (ifa->ifa_addr->sa_family == AF_INET)
				noinet = 0;
			continue;
		}
	}
	freeifaddrs(ifap);
	if (oname != NULL)
		free(oname);
	if (count == 0) {
		fprintf(stderr, "%s: no such interface\n", name);
		exit(1);
	}
}

/*ARGSUSED*/
void
clone_create(const char *addr, int param)
{

	/* We're called early... */
	getsock(AF_INET);

	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCIFCREATE, &ifr) == -1)
		err(1, "SIOCIFCREATE");
}

/*ARGSUSED*/
void
clone_destroy(const char *addr, int param)
{

	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCIFDESTROY, &ifr) == -1)
		err(1, "SIOCIFDESTROY");
}

void
list_cloners(void)
{
	struct if_clonereq ifcr;
	char *cp, *buf;
	int idx;

	memset(&ifcr, 0, sizeof(ifcr));

	getsock(AF_INET);

	if (ioctl(s, SIOCIFGCLONERS, &ifcr) == -1)
		err(1, "SIOCIFGCLONERS for count");

	buf = calloc(ifcr.ifcr_total, IFNAMSIZ);
	if (buf == NULL)
		err(1, "unable to allocate cloner name buffer");

	ifcr.ifcr_count = ifcr.ifcr_total;
	ifcr.ifcr_buffer = buf;

	if (ioctl(s, SIOCIFGCLONERS, &ifcr) == -1)
		err(1, "SIOCIFGCLONERS for names");

	/*
	 * In case some disappeared in the mean time, clamp it down.
	 */
	if (ifcr.ifcr_count > ifcr.ifcr_total)
		ifcr.ifcr_count = ifcr.ifcr_total;

	for (cp = buf, idx = 0; idx < ifcr.ifcr_count; idx++, cp += IFNAMSIZ) {
		if (idx > 0)
			putchar(' ');
		printf("%s", cp);
	}

	putchar('\n');
	free(buf);
}

#define RIDADDR 0
#define ADDR	1
#define MASK	2
#define DSTADDR	3

/*ARGSUSED*/
void
setifaddr(const char *addr, int param)
{
	/*
	 * Delay the ioctl to set the interface addr until flags are all set.
	 * The address interpretation may depend on the flags,
	 * and the flags may change when the address is set.
	 */
	setaddr++;
	if (doalias >= 0)
		newaddr = 1;
	if (doalias == 0)
		clearaddr = 1;
	(*afp->af_getaddr)(addr, (doalias >= 0 ? ADDR : RIDADDR));
}

#ifndef SMALL
void
setifrtlabel(const char *label, int d)
{
	if (d != 0)
		ifr.ifr_data = (caddr_t)(const char *)"";
	else
		ifr.ifr_data = (caddr_t)label;
	if (ioctl(s, SIOCSIFRTLABEL, &ifr) < 0)
		warn("SIOCSIFRTLABEL");
}
#endif

/* ARGSUSED */
void
setifnetmask(const char *addr, int ignored)
{
	(*afp->af_getaddr)(addr, MASK);
}

/* ARGSUSED */
void
setifbroadaddr(const char *addr, int ignored)
{
	(*afp->af_getaddr)(addr, DSTADDR);
}

/* ARGSUSED */
void
setifdesc(const char *val, int ignored)
{
	ifr.ifr_data = (caddr_t)val;
	if (ioctl(s, SIOCSIFDESCR, &ifr) < 0)
		warn("SIOCSIFDESCR");
}

/* ARGSUSED */
void
unsetifdesc(const char *noval, int ignored)
{
	ifr.ifr_data = (caddr_t)(const char *)"";
	if (ioctl(s, SIOCSIFDESCR, &ifr) < 0)
		warn("SIOCSIFDESCR");
}

/* ARGSUSED */
void
setifipdst(const char *addr, int ignored)
{
	in_getaddr(addr, DSTADDR);
	setipdst++;
	clearaddr = 0;
	newaddr = 0;
}

#define rqtosa(x) (&(((struct ifreq *)(afp->x))->ifr_addr))
/*ARGSUSED*/
void
notealias(const char *addr, int param)
{
	if (setaddr && doalias == 0 && param < 0)
		memcpy(rqtosa(af_ridreq), rqtosa(af_addreq),
		    rqtosa(af_addreq)->sa_len);
	doalias = param;
	if (param < 0) {
		clearaddr = 1;
		newaddr = 0;
	} else
		clearaddr = 0;
}

/*ARGSUSED*/
void
notrailers(const char *vname, int value)
{
	printf("Note: trailers are no longer sent, but always received\n");
}

/*ARGSUSED*/
void
setifdstaddr(const char *addr, int param)
{
	setaddr++;
	(*afp->af_getaddr)(addr, DSTADDR);
}

/*
 * Note: doing an SIOCGIFFLAGS scribbles on the union portion
 * of the ifreq structure, which may confuse other parts of ifconfig.
 * Make a private copy so we can avoid that.
 */
/* ARGSUSED */
void
setifflags(const char *vname, int value)
{
	struct ifreq my_ifr;

	bcopy((char *)&ifr, (char *)&my_ifr, sizeof(struct ifreq));

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&my_ifr) < 0)
		err(1, "SIOCGIFFLAGS");
	(void) strlcpy(my_ifr.ifr_name, name, sizeof(my_ifr.ifr_name));
	flags = my_ifr.ifr_flags;

	if (value < 0) {
		value = -value;
		flags &= ~value;
	} else
		flags |= value;
	my_ifr.ifr_flags = flags;
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&my_ifr) < 0)
		err(1, "SIOCSIFFLAGS");
}

/* ARGSUSED */
void
setifxflags(const char *vname, int value)
{
	struct ifreq my_ifr;

	if ((value == IFXF_INET6_PRIVACY || value == -IFXF_INET6_PRIVACY)
	    && afp->af_af != AF_INET6) {
		errx(1, "autoconfprivacy needs AF inet6, current AF is `%s'",
		    afp->af_name);
	}

	bcopy((char *)&ifr, (char *)&my_ifr, sizeof(struct ifreq));

	if (ioctl(s, SIOCGIFXFLAGS, (caddr_t)&my_ifr) < 0)
		warn("SIOCGIFXFLAGS");
	(void) strlcpy(my_ifr.ifr_name, name, sizeof(my_ifr.ifr_name));
	flags = my_ifr.ifr_flags;

	if (value < 0) {
		value = -value;
		flags &= ~value;
	} else
		flags |= value;
	my_ifr.ifr_flags = flags;
	if (ioctl(s, SIOCSIFXFLAGS, (caddr_t)&my_ifr) < 0)
		warn("SIOCSIFXFLAGS");
}

#ifdef INET6
void
setia6flags(const char *vname, int value)
{

	if (value < 0) {
		value = -value;
		in6_addreq.ifra_flags &= ~value;
	} else
		in6_addreq.ifra_flags |= value;
}

void
setia6pltime(const char *val, int d)
{

	setia6lifetime("pltime", val);
}

void
setia6vltime(const char *val, int d)
{

	setia6lifetime("vltime", val);
}

void
setia6lifetime(const char *cmd, const char *val)
{
	const char *errmsg = NULL;
	time_t newval, t;

	newval = strtonum(val, 0, 1000000, &errmsg);
	if (errmsg)
		errx(1, "invalid %s %s: %s", cmd, val, errmsg);

	t = time(NULL);

	if (afp->af_af != AF_INET6)
		errx(1, "%s not allowed for the AF", cmd);
	if (strcmp(cmd, "vltime") == 0) {
		in6_addreq.ifra_lifetime.ia6t_expire = t + newval;
		in6_addreq.ifra_lifetime.ia6t_vltime = newval;
	} else if (strcmp(cmd, "pltime") == 0) {
		in6_addreq.ifra_lifetime.ia6t_preferred = t + newval;
		in6_addreq.ifra_lifetime.ia6t_pltime = newval;
	}
}

void
setia6eui64(const char *cmd, int val)
{
	struct ifaddrs *ifap, *ifa;
	const struct sockaddr_in6 *sin6 = NULL;
	const struct in6_addr *lladdr = NULL;
	struct in6_addr *in6;

	if (afp->af_af != AF_INET6)
		errx(1, "%s not allowed for the AF", cmd);
	in6 = (struct in6_addr *)&in6_addreq.ifra_addr.sin6_addr;
	if (memcmp(&in6addr_any.s6_addr[8], &in6->s6_addr[8], 8) != 0)
		errx(1, "interface index is already filled");
	if (getifaddrs(&ifap) != 0)
		err(1, "getifaddrs");
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_INET6 &&
		    strcmp(ifa->ifa_name, name) == 0) {
			sin6 = (const struct sockaddr_in6 *)ifa->ifa_addr;
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
				lladdr = &sin6->sin6_addr;
				break;
			}
		}
	}
	if (!lladdr)
		errx(1, "could not determine link local address");

	memcpy(&in6->s6_addr[8], &lladdr->s6_addr[8], 8);

	freeifaddrs(ifap);
}
#endif /* INET6 */

/* ARGSUSED */
void
setifmetric(const char *val, int ignored)
{
	const char *errmsg = NULL;

	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

	ifr.ifr_metric = strtonum(val, 0, INT_MAX, &errmsg);
	if (errmsg)
		errx(1, "metric %s: %s", val, errmsg);
	if (ioctl(s, SIOCSIFMETRIC, (caddr_t)&ifr) < 0)
		warn("SIOCSIFMETRIC");
}

/* ARGSUSED */
void
setifmtu(const char *val, int d)
{
	const char *errmsg = NULL;

	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

	ifr.ifr_mtu = strtonum(val, 0, INT_MAX, &errmsg);
	if (errmsg)
		errx(1, "mtu %s: %s", val, errmsg);
	if (ioctl(s, SIOCSIFMTU, (caddr_t)&ifr) < 0)
		warn("SIOCSIFMTU");
}

/* ARGSUSED */
void
setifgroup(const char *group_name, int dummy)
{
	struct ifgroupreq ifgr;

	memset(&ifgr, 0, sizeof(ifgr));
	strlcpy(ifgr.ifgr_name, name, IFNAMSIZ);

	if (group_name[0] && isdigit(group_name[strlen(group_name) - 1]))
		errx(1, "setifgroup: group names may not end in a digit");

	if (strlcpy(ifgr.ifgr_group, group_name, IFNAMSIZ) >= IFNAMSIZ)
		errx(1, "setifgroup: group name too long");
	if (ioctl(s, SIOCAIFGROUP, (caddr_t)&ifgr) == -1) {
		if (errno != EEXIST)
			err(1," SIOCAIFGROUP");
	}
}

/* ARGSUSED */
void
unsetifgroup(const char *group_name, int dummy)
{
	struct ifgroupreq ifgr;

	memset(&ifgr, 0, sizeof(ifgr));
	strlcpy(ifgr.ifgr_name, name, IFNAMSIZ);

	if (group_name[0] && isdigit(group_name[strlen(group_name) - 1]))
		errx(1, "unsetifgroup: group names may not end in a digit");

	if (strlcpy(ifgr.ifgr_group, group_name, IFNAMSIZ) >= IFNAMSIZ)
		errx(1, "unsetifgroup: group name too long");
	if (ioctl(s, SIOCDIFGROUP, (caddr_t)&ifgr) == -1)
		err(1, "SIOCDIFGROUP");
}

const char *
get_string(const char *val, const char *sep, u_int8_t *buf, int *lenp)
{
	int len, hexstr;
	u_int8_t *p;

	len = *lenp;
	p = buf;
	hexstr = (val[0] == '0' && tolower((u_char)val[1]) == 'x');
	if (hexstr)
		val += 2;
	for (;;) {
		if (*val == '\0')
			break;
		if (sep != NULL && strchr(sep, *val) != NULL) {
			val++;
			break;
		}
		if (hexstr) {
			if (!isxdigit((u_char)val[0]) ||
			    !isxdigit((u_char)val[1])) {
				warnx("bad hexadecimal digits");
				return NULL;
			}
		}
		if (p > buf + len) {
			if (hexstr)
				warnx("hexadecimal digits too long");
			else
				warnx("strings too long");
			return NULL;
		}
		if (hexstr) {
#define	tohex(x)	(isdigit(x) ? (x) - '0' : tolower(x) - 'a' + 10)
			*p++ = (tohex((u_char)val[0]) << 4) |
			    tohex((u_char)val[1]);
#undef tohex
			val += 2;
		} else {
			if (*val == '\\' &&
			    sep != NULL && strchr(sep, *(val + 1)) != NULL)
				val++;
			*p++ = *val++;
		}
	}
	len = p - buf;
	if (len < *lenp)
		memset(p, 0, *lenp - len);
	*lenp = len;
	return val;
}

void
print_string(const u_int8_t *buf, int len)
{
	int i;
	int hasspc;

	i = 0;
	hasspc = 0;
	if (len < 2 || buf[0] != '0' || tolower(buf[1]) != 'x') {
		for (; i < len; i++) {
			/* Only print 7-bit ASCII keys */
			if (buf[i] & 0x80 || !isprint(buf[i]))
				break;
			if (isspace(buf[i]))
				hasspc++;
		}
	}
	if (i == len) {
		if (hasspc || len == 0)
			printf("\"%.*s\"", len, buf);
		else
			printf("%.*s", len, buf);
	} else {
		printf("0x");
		for (i = 0; i < len; i++)
			printf("%02x", buf[i]);
	}
}

void
setifnwid(const char *val, int d)
{
	struct ieee80211_nwid nwid;
	int len;

	if (d != 0) {
		/* no network id is especially desired */
		memset(&nwid, 0, sizeof(nwid));
		len = 0;
	} else {
		len = sizeof(nwid.i_nwid);
		if (get_string(val, NULL, nwid.i_nwid, &len) == NULL)
			return;
	}
	nwid.i_len = len;
	(void)strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)&nwid;
	if (ioctl(s, SIOCS80211NWID, (caddr_t)&ifr) < 0)
		warn("SIOCS80211NWID");
}

void
setifbssid(const char *val, int d)
{

	struct ieee80211_bssid bssid;
	struct ether_addr *ea;

	if (d != 0) {
		/* no BSSID is especially desired */
		memset(&bssid.i_bssid, 0, sizeof(bssid.i_bssid));
	} else {
		ea = ether_aton((char*)val);
		if (ea == NULL) {
			warnx("malformed BSSID: %s", val);
			return;
		}
		memcpy(&bssid.i_bssid, ea->ether_addr_octet,
		    sizeof(bssid.i_bssid));
	}
	strlcpy(bssid.i_name, name, sizeof(bssid.i_name));
	if (ioctl(s, SIOCS80211BSSID, &bssid) == -1)
		warn("SIOCS80211BSSID");
}

void
setifnwkey(const char *val, int d)
{
	int i, len;
	struct ieee80211_nwkey nwkey;
	u_int8_t keybuf[IEEE80211_WEP_NKID][16];

	bzero(&nwkey, sizeof(nwkey));
	bzero(&keybuf, sizeof(keybuf));

	nwkey.i_wepon = IEEE80211_NWKEY_WEP;
	nwkey.i_defkid = 1;
	if (d == -1) {
		/* disable WEP encryption */
		nwkey.i_wepon = 0;
		i = 0;
	} else if (strcasecmp("persist", val) == 0) {
		/* use all values from persistent memory */
		nwkey.i_wepon |= IEEE80211_NWKEY_PERSIST;
		nwkey.i_defkid = 0;
		for (i = 0; i < IEEE80211_WEP_NKID; i++)
			nwkey.i_key[i].i_keylen = -1;
	} else if (strncasecmp("persist:", val, 8) == 0) {
		val += 8;
		/* program keys in persistent memory */
		nwkey.i_wepon |= IEEE80211_NWKEY_PERSIST;
		goto set_nwkey;
	} else {
 set_nwkey:
		if (isdigit(val[0]) && val[1] == ':') {
			/* specifying a full set of four keys */
			nwkey.i_defkid = val[0] - '0';
			val += 2;
			for (i = 0; i < IEEE80211_WEP_NKID; i++) {
				len = sizeof(keybuf[i]);
				val = get_string(val, ",", keybuf[i], &len);
				if (val == NULL)
					return;
				nwkey.i_key[i].i_keylen = len;
				nwkey.i_key[i].i_keydat = keybuf[i];
			}
			if (*val != '\0') {
				warnx("SIOCS80211NWKEY: too many keys.");
				return;
			}
		} else {
			len = sizeof(keybuf[0]);
			val = get_string(val, NULL, keybuf[0], &len);
			if (val == NULL)
				return;
			nwkey.i_key[0].i_keylen = len;
			nwkey.i_key[0].i_keydat = keybuf[0];
			i = 1;
		}
	}
	(void)strlcpy(nwkey.i_name, name, sizeof(nwkey.i_name));
	if (ioctl(s, SIOCS80211NWKEY, (caddr_t)&nwkey) == -1)
		warn("SIOCS80211NWKEY");
}

/* ARGSUSED */
void
setifwpa(const char *val, int d)
{
	struct ieee80211_wpaparams wpa;

	(void)strlcpy(wpa.i_name, name, sizeof(wpa.i_name));
	if (ioctl(s, SIOCG80211WPAPARMS, (caddr_t)&wpa) < 0)
		err(1, "SIOCG80211WPAPARMS");
	wpa.i_enabled = d;
	if (ioctl(s, SIOCS80211WPAPARMS, (caddr_t)&wpa) < 0)
		err(1, "SIOCS80211WPAPARMS");
}

/* ARGSUSED */
void
setifwpaprotos(const char *val, int d)
{
	struct ieee80211_wpaparams wpa;
	char *optlist, *str;
	u_int rval = 0;

	if ((optlist = strdup(val)) == NULL)
		err(1, "strdup");
	str = strtok(optlist, ",");
	while (str != NULL) {
		if (strcasecmp(str, "wpa1") == 0)
			rval |= IEEE80211_WPA_PROTO_WPA1;
		else if (strcasecmp(str, "wpa2") == 0)
			rval |= IEEE80211_WPA_PROTO_WPA2;
		else
			errx(1, "wpaprotos: unknown protocol: %s", str);
		str = strtok(NULL, ",");
	}
	free(optlist);

	(void)strlcpy(wpa.i_name, name, sizeof(wpa.i_name));
	if (ioctl(s, SIOCG80211WPAPARMS, (caddr_t)&wpa) < 0)
		err(1, "SIOCG80211WPAPARMS");
	wpa.i_protos = rval;
	if (ioctl(s, SIOCS80211WPAPARMS, (caddr_t)&wpa) < 0)
		err(1, "SIOCS80211WPAPARMS");
}

/* ARGSUSED */
void
setifwpaakms(const char *val, int d)
{
	struct ieee80211_wpaparams wpa;
	char *optlist, *str;
	u_int rval = 0;

	if ((optlist = strdup(val)) == NULL)
		err(1, "strdup");
	str = strtok(optlist, ",");
	while (str != NULL) {
		if (strcasecmp(str, "psk") == 0)
			rval |= IEEE80211_WPA_AKM_PSK;
		else if (strcasecmp(str, "802.1x") == 0)
			rval |= IEEE80211_WPA_AKM_8021X;
		else
			errx(1, "wpaakms: unknown akm: %s", str);
		str = strtok(NULL, ",");
	}
	free(optlist);

	(void)strlcpy(wpa.i_name, name, sizeof(wpa.i_name));
	if (ioctl(s, SIOCG80211WPAPARMS, (caddr_t)&wpa) < 0)
		err(1, "SIOCG80211WPAPARMS");
	wpa.i_akms = rval;
	if (ioctl(s, SIOCS80211WPAPARMS, (caddr_t)&wpa) < 0)
		err(1, "SIOCS80211WPAPARMS");
}

static const struct {
	const char	*name;
	u_int		cipher;
} ciphers[] = {
	{ "usegroup",	IEEE80211_WPA_CIPHER_USEGROUP },
	{ "wep40",	IEEE80211_WPA_CIPHER_WEP40 },
	{ "tkip",	IEEE80211_WPA_CIPHER_TKIP },
	{ "ccmp",	IEEE80211_WPA_CIPHER_CCMP },
	{ "wep104",	IEEE80211_WPA_CIPHER_WEP104 }
};

u_int
getwpacipher(const char *name)
{
	int i;

	for (i = 0; i < sizeof(ciphers) / sizeof(ciphers[0]); i++)
		if (strcasecmp(name, ciphers[i].name) == 0)
			return ciphers[i].cipher;
	return IEEE80211_WPA_CIPHER_NONE;
}

/* ARGSUSED */
void
setifwpaciphers(const char *val, int d)
{
	struct ieee80211_wpaparams wpa;
	char *optlist, *str;
	u_int rval = 0;

	if ((optlist = strdup(val)) == NULL)
		err(1, "strdup");
	str = strtok(optlist, ",");
	while (str != NULL) {
		u_int cipher = getwpacipher(str);
		if (cipher == IEEE80211_WPA_CIPHER_NONE)
			errx(1, "wpaciphers: unknown cipher: %s", str);

		rval |= cipher;
		str = strtok(NULL, ",");
	}
	free(optlist);

	(void)strlcpy(wpa.i_name, name, sizeof(wpa.i_name));
	if (ioctl(s, SIOCG80211WPAPARMS, (caddr_t)&wpa) < 0)
		err(1, "SIOCG80211WPAPARMS");
	wpa.i_ciphers = rval;
	if (ioctl(s, SIOCS80211WPAPARMS, (caddr_t)&wpa) < 0)
		err(1, "SIOCS80211WPAPARMS");
}

/* ARGSUSED */
void
setifwpagroupcipher(const char *val, int d)
{
	struct ieee80211_wpaparams wpa;
	u_int cipher;

	cipher = getwpacipher(val);
	if (cipher == IEEE80211_WPA_CIPHER_NONE)
		errx(1, "wpagroupcipher: unknown cipher: %s", val);

	(void)strlcpy(wpa.i_name, name, sizeof(wpa.i_name));
	if (ioctl(s, SIOCG80211WPAPARMS, (caddr_t)&wpa) < 0)
		err(1, "SIOCG80211WPAPARMS");
	wpa.i_groupcipher = cipher;
	if (ioctl(s, SIOCS80211WPAPARMS, (caddr_t)&wpa) < 0)
		err(1, "SIOCS80211WPAPARMS");
}

void
setifwpapsk(const char *val, int d)
{
	struct ieee80211_wpapsk psk;
	int len;

	if (d != -1) {
		len = sizeof(psk.i_psk);
		val = get_string(val, NULL, psk.i_psk, &len);
		if (val == NULL)
			errx(1, "wpapsk: invalid pre-shared key");
		if (len != sizeof(psk.i_psk))
			errx(1, "wpapsk: bad pre-shared key length");
		psk.i_enabled = 1;
	} else
		psk.i_enabled = 0;

	(void)strlcpy(psk.i_name, name, sizeof(psk.i_name));
	if (ioctl(s, SIOCS80211WPAPSK, (caddr_t)&psk) < 0)
		err(1, "SIOCS80211WPAPSK");
}

void
setifchan(const char *val, int d)
{
	struct ieee80211chanreq channel;
	const char *errstr;
	int chan;

	if (val == NULL) {
		if (shownet80211chans || shownet80211nodes)
			usage(1);
		shownet80211chans = 1;
		return;
	}
	if (d != 0)
		chan = IEEE80211_CHAN_ANY;
	else {
		chan = strtonum(val, 1, 256, &errstr);
		if (errstr) {
			warnx("invalid channel %s: %s", val, errstr);
			return;
		}
	}

	strlcpy(channel.i_name, name, sizeof(channel.i_name));
	channel.i_channel = (u_int16_t)chan;
	if (ioctl(s, SIOCS80211CHANNEL, (caddr_t)&channel) == -1)
		warn("SIOCS80211CHANNEL");
}

/* ARGSUSED */
void
setifscan(const char *val, int d)
{
	if (shownet80211chans || shownet80211nodes)
		usage(1);
	shownet80211nodes = 1;
}

#ifndef SMALL
void
setiftxpower(const char *val, int d)
{
	const char *errmsg = NULL;
	struct ieee80211_txpower txpower;
	int dbm;

	strlcpy(txpower.i_name, name, sizeof(txpower.i_name));

	if (d == 1) {
		txpower.i_mode = IEEE80211_TXPOWER_MODE_AUTO;
	} else {
		dbm = strtonum(val, SHRT_MIN, SHRT_MAX, &errmsg);
		if (errmsg)
			errx(1, "txpower %sdBm: %s", val, errmsg);
		txpower.i_val = (int16_t)dbm;
		txpower.i_mode = IEEE80211_TXPOWER_MODE_FIXED;
	}

	if (ioctl(s, SIOCS80211TXPOWER, (caddr_t)&txpower) == -1)
		warn("SIOCS80211TXPOWER");
}

void
setifnwflag(const char *val, int d)
{
	static const struct ieee80211_flags nwflags[] = IEEE80211_FLAGS;
	u_int i, flag = 0;

	for (i = 0; i < (sizeof(nwflags) / sizeof(nwflags[0])); i++) {
		if (strcmp(val, nwflags[i].f_name) == 0) {
			flag = nwflags[i].f_flag;
			break;
		}
	}
	if (flag == 0)
		errx(1, "Invalid nwflag: %s", val);

	if (ioctl(s, SIOCG80211FLAGS, (caddr_t)&ifr) != 0)
		err(1, "SIOCG80211FLAGS");

	if (d)
		ifr.ifr_flags &= ~flag;
	else
		ifr.ifr_flags |= flag;

	if (ioctl(s, SIOCS80211FLAGS, (caddr_t)&ifr) != 0)
		err(1, "SIOCS80211FLAGS");
}

void
unsetifnwflag(const char *val, int d)
{
	setifnwflag(val, 1);
}
#endif

/* ARGSUSED */
void
setifpowersave(const char *val, int d)
{
	struct ieee80211_power power;
	const char *errmsg = NULL;

	(void)strlcpy(power.i_name, name, sizeof(power.i_name));
	if (ioctl(s, SIOCG80211POWER, (caddr_t)&power) == -1) {
		warn("SIOCG80211POWER");
		return;
	}

	if (d != -1 && val != NULL) {
		power.i_maxsleep = strtonum(val, 0, INT_MAX, &errmsg);
		if (errmsg)
			errx(1, "powersave %s: %s", val, errmsg);
	}

	power.i_enabled = d == -1 ? 0 : 1;
	if (ioctl(s, SIOCS80211POWER, (caddr_t)&power) == -1)
		warn("SIOCS80211POWER");
}

void
print_cipherset(u_int32_t cipherset)
{
	const char *sep = "";
	int i;

	if (cipherset == IEEE80211_WPA_CIPHER_NONE) {
		printf("none");
		return;
	}
	for (i = 0; i < sizeof(ciphers) / sizeof(ciphers[0]); i++) {
		if (cipherset & ciphers[i].cipher) {
			printf("%s%s", sep, ciphers[i].name);
			sep = ",";
		}
	}
}

void
ieee80211_status(void)
{
	int len, i, nwkey_verbose, inwid, inwkey, ipsk, ichan, ipwr;
	int ibssid, itxpower, iwpa;
	struct ieee80211_nwid nwid;
	struct ieee80211_nwkey nwkey;
	struct ieee80211_wpapsk psk;
	struct ieee80211_power power;
	struct ieee80211chanreq channel;
	struct ieee80211_bssid bssid;
	struct ieee80211_txpower txpower;
	struct ieee80211_wpaparams wpa;
	struct ieee80211_nodereq nr;
	u_int8_t zero_bssid[IEEE80211_ADDR_LEN];
	u_int8_t keybuf[IEEE80211_WEP_NKID][16];
	struct ether_addr ea;

	/* get current status via ioctls */
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_data = (caddr_t)&nwid;
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	inwid = ioctl(s, SIOCG80211NWID, (caddr_t)&ifr);

	memset(&nwkey, 0, sizeof(nwkey));
	strlcpy(nwkey.i_name, name, sizeof(nwkey.i_name));
	inwkey = ioctl(s, SIOCG80211NWKEY, (caddr_t)&nwkey);

	memset(&psk, 0, sizeof(psk));
	strlcpy(psk.i_name, name, sizeof(psk.i_name));
	ipsk = ioctl(s, SIOCG80211WPAPSK, (caddr_t)&psk);

	memset(&power, 0, sizeof(power));
	strlcpy(power.i_name, name, sizeof(power.i_name));
	ipwr = ioctl(s, SIOCG80211POWER, &power);

	memset(&channel, 0, sizeof(channel));
	strlcpy(channel.i_name, name, sizeof(channel.i_name));
	ichan = ioctl(s, SIOCG80211CHANNEL, (caddr_t)&channel);

	memset(&bssid, 0, sizeof(bssid));
	strlcpy(bssid.i_name, name, sizeof(bssid.i_name));
	ibssid = ioctl(s, SIOCG80211BSSID, &bssid);

	memset(&txpower, 0, sizeof(txpower));
	strlcpy(txpower.i_name, name, sizeof(txpower.i_name));
	itxpower = ioctl(s, SIOCG80211TXPOWER, &txpower);

	memset(&wpa, 0, sizeof(wpa));
	strlcpy(wpa.i_name, name, sizeof(wpa.i_name));
	iwpa = ioctl(s, SIOCG80211WPAPARMS, &wpa);

	/* check if any ieee80211 option is active */
	if (inwid == 0 || inwkey == 0 || ipsk == 0 || ipwr == 0 ||
	    ichan == 0 || ibssid == 0 || iwpa == 0 || itxpower == 0)
		fputs("\tieee80211:", stdout);
	else
		return;

	if (inwid == 0) {
		/* nwid.i_nwid is not NUL terminated. */
		len = nwid.i_len;
		if (len > IEEE80211_NWID_LEN)
			len = IEEE80211_NWID_LEN;
		fputs(" nwid ", stdout);
		print_string(nwid.i_nwid, len);
	}

	if (ichan == 0 && channel.i_channel != 0 &&
	    channel.i_channel != IEEE80211_CHAN_ANY)
		printf(" chan %u", channel.i_channel);

	memset(&zero_bssid, 0, sizeof(zero_bssid));
	if (ibssid == 0 &&
	    memcmp(bssid.i_bssid, zero_bssid, IEEE80211_ADDR_LEN) != 0) {
		memcpy(&ea.ether_addr_octet, bssid.i_bssid,
		    sizeof(ea.ether_addr_octet));
		printf(" bssid %s", ether_ntoa(&ea));

		bzero(&nr, sizeof(nr));
		bcopy(bssid.i_bssid, &nr.nr_macaddr, sizeof(nr.nr_macaddr));
		strlcpy(nr.nr_ifname, name, sizeof(nr.nr_ifname));
		if (ioctl(s, SIOCG80211NODE, &nr) == 0 && nr.nr_rssi) {
			if (nr.nr_max_rssi)
				printf(" %u%%", IEEE80211_NODEREQ_RSSI(&nr));
			else
				printf(" %udB", nr.nr_rssi);
		}
	}

	if (inwkey == 0 && nwkey.i_wepon > 0) {
		fputs(" nwkey ", stdout);
		/* try to retrieve WEP keys */
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			nwkey.i_key[i].i_keydat = keybuf[i];
			nwkey.i_key[i].i_keylen = sizeof(keybuf[i]);
		}
		if (ioctl(s, SIOCG80211NWKEY, (caddr_t)&nwkey) == -1) {
			fputs("<not displayed>", stdout);
		} else {
			nwkey_verbose = 0;
			/*
			 * check to see non default key
			 * or multiple keys defined
			 */
			if (nwkey.i_defkid != 1) {
				nwkey_verbose = 1;
			} else {
				for (i = 1; i < IEEE80211_WEP_NKID; i++) {
					if (nwkey.i_key[i].i_keylen != 0) {
						nwkey_verbose = 1;
						break;
					}
				}
			}
			/* check extra ambiguity with keywords */
			if (!nwkey_verbose) {
				if (nwkey.i_key[0].i_keylen >= 2 &&
				    isdigit(nwkey.i_key[0].i_keydat[0]) &&
				    nwkey.i_key[0].i_keydat[1] == ':')
					nwkey_verbose = 1;
				else if (nwkey.i_key[0].i_keylen >= 7 &&
				    strncasecmp("persist",
				    (char *)nwkey.i_key[0].i_keydat, 7) == 0)
					nwkey_verbose = 1;
			}
			if (nwkey_verbose)
				printf("%d:", nwkey.i_defkid);
			for (i = 0; i < IEEE80211_WEP_NKID; i++) {
				if (i > 0)
					putchar(',');
				if (nwkey.i_key[i].i_keylen < 0) {
					fputs("persist", stdout);
				} else {
					/*
					 * XXX
					 * sanity check nwkey.i_key[i].i_keylen
					 */
					print_string(nwkey.i_key[i].i_keydat,
					    nwkey.i_key[i].i_keylen);
				}
				if (!nwkey_verbose)
					break;
			}
		}
	}

	if (ipsk == 0 && psk.i_enabled) {
		fputs(" wpapsk ", stdout);
		if (psk.i_enabled == 2)
			fputs("<not displayed>", stdout);
		else
			print_string(psk.i_psk, sizeof(psk.i_psk));
	}
	if (iwpa == 0 && wpa.i_enabled) {
		const char *sep;

		fputs(" wpaprotos ", stdout); sep = "";
		if (wpa.i_protos & IEEE80211_WPA_PROTO_WPA1) {
			fputs("wpa1", stdout);
			sep = ",";
		}
		if (wpa.i_protos & IEEE80211_WPA_PROTO_WPA2)
			printf("%swpa2", sep);

		fputs(" wpaakms ", stdout); sep = "";
		if (wpa.i_akms & IEEE80211_WPA_AKM_PSK) {
			fputs("psk", stdout);
			sep = ",";
		}
		if (wpa.i_akms & IEEE80211_WPA_AKM_8021X)
			printf("%s802.1x", sep);

		fputs(" wpaciphers ", stdout);
		print_cipherset(wpa.i_ciphers);

		fputs(" wpagroupcipher ", stdout);
		print_cipherset(wpa.i_groupcipher);
	}

	if (ipwr == 0 && power.i_enabled)
		printf(" powersave on (%dms sleep)", power.i_maxsleep);

	if (itxpower == 0)
		printf(" %ddBm%s", txpower.i_val,
		    txpower.i_mode == IEEE80211_TXPOWER_MODE_AUTO ?
		    " (auto)" : "");

	if (ioctl(s, SIOCG80211FLAGS, (caddr_t)&ifr) == 0 &&
	    ifr.ifr_flags) {
		putchar(' ');
		printb_status(ifr.ifr_flags, IEEE80211_F_USERBITS);
	}

	putchar('\n');
	if (shownet80211chans)
		ieee80211_listchans();
	else if (shownet80211nodes)
		ieee80211_listnodes();
}

void
ieee80211_listchans(void)
{
	static struct ieee80211_channel chans[256+1];
	struct ieee80211_chanreq_all ca;
	int i;

	bzero(&ca, sizeof(ca));
	bzero(chans, sizeof(chans));
	ca.i_chans = chans;
	strlcpy(ca.i_name, name, sizeof(ca.i_name));

	if (ioctl(s, SIOCG80211ALLCHANS, &ca) != 0) {
		warn("SIOCG80211ALLCHANS");
		return;
	}
	printf("\t\t%4s  %-8s  %s\n", "chan", "freq", "properties");
	for (i = 1; i <= 256; i++) {
		if (chans[i].ic_flags == 0)
			continue;
		printf("\t\t%4d  %4d MHz  ", i, chans[i].ic_freq);
		if (chans[i].ic_flags & IEEE80211_CHAN_PASSIVE)
			printf("passive scan");
		else
			putchar('-');
		putchar('\n');
	}
}

void
ieee80211_listnodes(void)
{
	struct ieee80211_nodereq_all na;
	struct ieee80211_nodereq nr[512];
	struct ifreq ifr;
	int i, down = 0;

	if ((flags & IFF_UP) == 0) {
		down = 1;
		setifflags("up", IFF_UP);
	}

	bzero(&ifr, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

	if (ioctl(s, SIOCS80211SCAN, (caddr_t)&ifr) != 0) {
		if (errno == EPERM)
			printf("\t\tno permission to scan\n");
		goto done;
	}

	bzero(&na, sizeof(na));
	bzero(&nr, sizeof(nr));
	na.na_node = nr;
	na.na_size = sizeof(nr);
	strlcpy(na.na_ifname, name, sizeof(na.na_ifname));

	if (ioctl(s, SIOCG80211ALLNODES, &na) != 0) {
		warn("SIOCG80211ALLNODES");
		goto done;
	}

	if (!na.na_nodes)
		printf("\t\tnone\n");

	for (i = 0; i < na.na_nodes; i++) {
		printf("\t\t");
		ieee80211_printnode(&nr[i]);
		putchar('\n');
	}

 done:
	if (down)
		setifflags("restore", -IFF_UP);
}

void
ieee80211_printnode(struct ieee80211_nodereq *nr)
{
	int len;

	if (nr->nr_flags & IEEE80211_NODEREQ_AP) {
		len = nr->nr_nwid_len;
		if (len > IEEE80211_NWID_LEN)
			len = IEEE80211_NWID_LEN;
		printf("nwid ");
		print_string(nr->nr_nwid, len);
		putchar(' ');

		printf("chan %u ", nr->nr_channel);
	}

	if (nr->nr_flags & IEEE80211_NODEREQ_AP)
		printf("bssid %s ",
		    ether_ntoa((struct ether_addr*)nr->nr_bssid));
	else
		printf("lladdr %s ",
		    ether_ntoa((struct ether_addr*)nr->nr_macaddr));

	if (nr->nr_max_rssi)
		printf("%u%% ", IEEE80211_NODEREQ_RSSI(nr));
	else
		printf("%udB ", nr->nr_rssi);

	if (nr->nr_pwrsave)
		printf("powersave ");
	if (nr->nr_nrates) {
		/* Only print the fastest rate */
		printf("%uM",
		    (nr->nr_rates[nr->nr_nrates - 1] & IEEE80211_RATE_VAL) / 2);
		putchar(' ');
	}
	/* ESS is the default, skip it */
	nr->nr_capinfo &= ~IEEE80211_CAPINFO_ESS;
	if (nr->nr_capinfo) {
		printb_status(nr->nr_capinfo, IEEE80211_CAPINFO_BITS);
		putchar(' ');
	}

	if ((nr->nr_flags & IEEE80211_NODEREQ_AP) == 0)
		printb_status(IEEE80211_NODEREQ_STATE(nr->nr_state),
		    IEEE80211_NODEREQ_STATE_BITS);
}

void
init_current_media(void)
{
	struct ifmediareq ifmr;

	/*
	 * If we have not yet done so, grab the currently-selected
	 * media.
	 */
	if ((actions & (A_MEDIA|A_MEDIAOPT|A_MEDIAMODE)) == 0) {
		(void) memset(&ifmr, 0, sizeof(ifmr));
		(void) strlcpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));

		if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
			/*
			 * If we get E2BIG, the kernel is telling us
			 * that there are more, so we can ignore it.
			 */
			if (errno != E2BIG)
				err(1, "SIOCGIFMEDIA");
		}

		media_current = ifmr.ifm_current;
	}

	/* Sanity. */
	if (IFM_TYPE(media_current) == 0)
		errx(1, "%s: no link type?", name);
}

void
process_media_commands(void)
{

	if ((actions & (A_MEDIA|A_MEDIAOPT)) == 0) {
		/* Nothing to do. */
		return;
	}

	/*
	 * Media already set up, and commands sanity-checked.  Set/clear
	 * any options, and we're ready to go.
	 */
	media_current |= mediaopt_set;
	media_current &= ~mediaopt_clear;

	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_media = media_current;

	if (ioctl(s, SIOCSIFMEDIA, (caddr_t)&ifr) < 0)
		;
}

/* ARGSUSED */
void
setmedia(const char *val, int d)
{
	int type, subtype, inst;

	if (val == NULL) {
		if (showmediaflag)
			usage(1);
		showmediaflag = 1;
		return;
	}

	init_current_media();

	/* Only one media command may be given. */
	if (actions & A_MEDIA)
		errx(1, "only one `media' command may be issued");

	/* Must not come after mode commands */
	if (actions & A_MEDIAMODE)
		errx(1, "may not issue `media' after `mode' commands");

	/* Must not come after mediaopt commands */
	if (actions & A_MEDIAOPT)
		errx(1, "may not issue `media' after `mediaopt' commands");

	/*
	 * No need to check if `instance' has been issued; setmediainst()
	 * craps out if `media' has not been specified.
	 */

	type = IFM_TYPE(media_current);
	inst = IFM_INST(media_current);

	/* Look up the subtype. */
	subtype = get_media_subtype(type, val);

	/* Build the new current media word. */
	media_current = IFM_MAKEWORD(type, subtype, 0, inst);

	/* Media will be set after other processing is complete. */
}

/* ARGSUSED */
void
setmediamode(const char *val, int d)
{
	int type, subtype, options, inst, mode;

	init_current_media();

	/* Can only issue `mode' once. */
	if (actions & A_MEDIAMODE)
		errx(1, "only one `mode' command may be issued");

	type = IFM_TYPE(media_current);
	subtype = IFM_SUBTYPE(media_current);
	options = IFM_OPTIONS(media_current);
	inst = IFM_INST(media_current);

	if ((mode = get_media_mode(type, val)) == -1)
		errx(1, "invalid media mode: %s", val);
	media_current = IFM_MAKEWORD(type, subtype, options, inst) | mode;
	/* Media will be set after other processing is complete. */
}

void
setmediaopt(const char *val, int d)
{

	init_current_media();

	/* Can only issue `mediaopt' once. */
	if (actions & A_MEDIAOPTSET)
		errx(1, "only one `mediaopt' command may be issued");

	/* Can't issue `mediaopt' if `instance' has already been issued. */
	if (actions & A_MEDIAINST)
		errx(1, "may not issue `mediaopt' after `instance'");

	mediaopt_set = get_media_options(IFM_TYPE(media_current), val);

	/* Media will be set after other processing is complete. */
}

/* ARGSUSED */
void
unsetmediaopt(const char *val, int d)
{

	init_current_media();

	/* Can only issue `-mediaopt' once. */
	if (actions & A_MEDIAOPTCLR)
		errx(1, "only one `-mediaopt' command may be issued");

	/* May not issue `media' and `-mediaopt'. */
	if (actions & A_MEDIA)
		errx(1, "may not issue both `media' and `-mediaopt'");

	/*
	 * No need to check for A_MEDIAINST, since the test for A_MEDIA
	 * implicitly checks for A_MEDIAINST.
	 */

	mediaopt_clear = get_media_options(IFM_TYPE(media_current), val);

	/* Media will be set after other processing is complete. */
}

/* ARGSUSED */
void
setmediainst(const char *val, int d)
{
	int type, subtype, options, inst;
	const char *errmsg = NULL;

	init_current_media();

	/* Can only issue `instance' once. */
	if (actions & A_MEDIAINST)
		errx(1, "only one `instance' command may be issued");

	/* Must have already specified `media' */
	if ((actions & A_MEDIA) == 0)
		errx(1, "must specify `media' before `instance'");

	type = IFM_TYPE(media_current);
	subtype = IFM_SUBTYPE(media_current);
	options = IFM_OPTIONS(media_current);

	inst = strtonum(val, 0, IFM_INST_MAX, &errmsg);
	if (errmsg)
		errx(1, "media instance %s: %s", val, errmsg);

	media_current = IFM_MAKEWORD(type, subtype, options, inst);

	/* Media will be set after other processing is complete. */
}

/*
 * Note: 
 * bits:       0   1   2   3   4   5   ....   24   25   ...   30   31
 * T1 mode:   N/A ch1 ch2 ch3 ch4 ch5        ch24  N/A        N/A  N/A
 * E1 mode:   ts0 ts1 ts2 ts3 ts4 ts5        ts24  ts25       ts30 ts31
 */
#ifndef SMALL
/* ARGSUSED */
void
settimeslot(const char *val, int d)
{
#define SINGLE_CHANNEL	0x1
#define RANGE_CHANNEL	0x2
#define ALL_CHANNELS	0xFFFFFFFF
	unsigned long	ts_map = 0;
	char	*ptr = (char*)val;
	int		ts_flag = 0;
	int		ts = 0, ts_start = 0;

	if (strcmp(val,"all") == 0) {
		ts_map = ALL_CHANNELS;
	} else {
		while (*ptr != '\0') {
			if (isdigit(*ptr)) {
				ts = strtoul(ptr, &ptr, 10);
				ts_flag |= SINGLE_CHANNEL;
			} else {
				if (*ptr == '-') {
					ts_flag |= RANGE_CHANNEL;
					ts_start = ts;
				} else {
					ts_map |= get_ts_map(ts_flag,
					    ts_start, ts);
					ts_flag = 0;
				}
				ptr++;
			}
		}
		if (ts_flag)
			ts_map |= get_ts_map(ts_flag, ts_start, ts);

	}
	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)&ts_map;

	if (ioctl(s, SIOCSIFTIMESLOT, (caddr_t)&ifr) < 0)
		err(1, "SIOCSIFTIMESLOT");
}

unsigned long
get_ts_map(int ts_flag, int ts_start, int ts_stop)
{
	int		i = 0;
	unsigned long	map = 0, mask = 0;

	if ((ts_flag & (SINGLE_CHANNEL | RANGE_CHANNEL)) == 0)
		return 0;
	if (ts_flag & RANGE_CHANNEL) { /* Range of channels */
		for (i = ts_start; i <= ts_stop; i++) {
			mask = 1 << i;
			map |=mask;
		}
	} else { /* Single channel */
		mask = 1 << ts_stop;
		map |= mask;
	}
	return map;
}
#endif /* SMALL */

#ifndef SMALL
void
timeslot_status(void)
{
	char		*sep = " ";
	unsigned long	 ts_map = 0;
	int		 i, start = -1;

	ifr.ifr_data = (caddr_t)&ts_map;

	if (ioctl(s, SIOCGIFTIMESLOT, (caddr_t)&ifr) == -1)
		return;

	printf("\ttimeslot:");
	for (i = 0; i < sizeof(ts_map) * 8; i++) {
		if (start == -1 && ts_map & (1 << i))
			start = i;
		else if (start != -1 && !(ts_map & (1 << i))) {
			if (start == i - 1)
				printf("%s%d", sep, start);
			else
				printf("%s%d-%d", sep, start, i-1);
			sep = ",";
			start = -1;
		}
	}
	if (start != -1) {
		if (start == i - 1)
			printf("%s%d", sep, start);
		else
			printf("%s%d-%d", sep, start, i-1);
	}
	printf("\n");
}
#endif


const struct ifmedia_description ifm_type_descriptions[] =
    IFM_TYPE_DESCRIPTIONS;

const struct ifmedia_description ifm_subtype_descriptions[] =
    IFM_SUBTYPE_DESCRIPTIONS;

struct ifmedia_description ifm_mode_descriptions[] =
    IFM_MODE_DESCRIPTIONS;

const struct ifmedia_description ifm_option_descriptions[] =
    IFM_OPTION_DESCRIPTIONS;

const char *
get_media_type_string(int mword)
{
	const struct ifmedia_description *desc;

	for (desc = ifm_type_descriptions; desc->ifmt_string != NULL;
	    desc++) {
		if (IFM_TYPE(mword) == desc->ifmt_word)
			return (desc->ifmt_string);
	}
	return ("<unknown type>");
}

const char *
get_media_subtype_string(int mword)
{
	const struct ifmedia_description *desc;

	for (desc = ifm_subtype_descriptions; desc->ifmt_string != NULL;
	    desc++) {
		if (IFM_TYPE_MATCH(desc->ifmt_word, mword) &&
		    IFM_SUBTYPE(desc->ifmt_word) == IFM_SUBTYPE(mword))
			return (desc->ifmt_string);
	}
	return ("<unknown subtype>");
}

int
get_media_subtype(int type, const char *val)
{
	int rval;

	rval = lookup_media_word(ifm_subtype_descriptions, type, val);
	if (rval == -1)
		errx(1, "unknown %s media subtype: %s",
		    get_media_type_string(type), val);

	return (rval);
}

int
get_media_mode(int type, const char *val)
{
	int rval;

	rval = lookup_media_word(ifm_mode_descriptions, type, val);
	if (rval == -1)
		errx(1, "unknown %s media mode: %s",
		    get_media_type_string(type), val);
	return (rval);
}

int
get_media_options(int type, const char *val)
{
	char *optlist, *str;
	int option, rval = 0;

	/* We muck with the string, so copy it. */
	optlist = strdup(val);
	if (optlist == NULL)
		err(1, "strdup");
	str = optlist;

	/*
	 * Look up the options in the user-provided comma-separated list.
	 */
	for (; (str = strtok(str, ",")) != NULL; str = NULL) {
		option = lookup_media_word(ifm_option_descriptions, type, str);
		if (option == -1)
			errx(1, "unknown %s media option: %s",
			    get_media_type_string(type), str);
		rval |= IFM_OPTIONS(option);
	}

	free(optlist);
	return (rval);
}

int
lookup_media_word(const struct ifmedia_description *desc, int type,
    const char *val)
{

	for (; desc->ifmt_string != NULL; desc++) {
		if (IFM_TYPE_MATCH(desc->ifmt_word, type) &&
		    strcasecmp(desc->ifmt_string, val) == 0)
			return (desc->ifmt_word);
	}
	return (-1);
}

void
print_media_word(int ifmw, int print_type, int as_syntax)
{
	const struct ifmedia_description *desc;
	int seen_option = 0;

	if (print_type)
		printf("%s ", get_media_type_string(ifmw));
	printf("%s%s", as_syntax ? "media " : "",
	    get_media_subtype_string(ifmw));

	/* Find mode. */
	if (IFM_MODE(ifmw) != 0) {
		for (desc = ifm_mode_descriptions; desc->ifmt_string != NULL;
		    desc++) {
			if (IFM_TYPE_MATCH(desc->ifmt_word, ifmw) &&
			    IFM_MODE(ifmw) == IFM_MODE(desc->ifmt_word)) {
				printf(" mode %s", desc->ifmt_string);
				break;
			}
		}
	}

	/* Find options. */
	for (desc = ifm_option_descriptions; desc->ifmt_string != NULL;
	    desc++) {
		if (IFM_TYPE_MATCH(desc->ifmt_word, ifmw) &&
		    (IFM_OPTIONS(ifmw) & IFM_OPTIONS(desc->ifmt_word)) != 0 &&
		    (seen_option & IFM_OPTIONS(desc->ifmt_word)) == 0) {
			if (seen_option == 0)
				printf(" %s", as_syntax ? "mediaopt " : "");
			printf("%s%s", seen_option ? "," : "",
			    desc->ifmt_string);
			seen_option |= IFM_OPTIONS(desc->ifmt_word);
		}
	}
	if (IFM_INST(ifmw) != 0)
		printf(" instance %d", IFM_INST(ifmw));
}

/* ARGSUSED */
static void
phys_status(int force)
{
	char psrcaddr[NI_MAXHOST];
	char pdstaddr[NI_MAXHOST];
	const char *ver = "";
	const int niflag = NI_NUMERICHOST;
	struct if_laddrreq req;

	psrcaddr[0] = pdstaddr[0] = '\0';

	memset(&req, 0, sizeof(req));
	(void) strlcpy(req.iflr_name, name, sizeof(req.iflr_name));
	if (ioctl(s, SIOCGLIFPHYADDR, (caddr_t)&req) < 0)
		return;
#ifdef INET6
	if (req.addr.ss_family == AF_INET6)
		in6_fillscopeid((struct sockaddr_in6 *)&req.addr);
#endif /* INET6 */
	if (getnameinfo((struct sockaddr *)&req.addr, req.addr.ss_len,
	    psrcaddr, sizeof(psrcaddr), 0, 0, niflag) != 0)
		strlcpy(psrcaddr, "<error>", sizeof(psrcaddr));
#ifdef INET6
	if (req.addr.ss_family == AF_INET6)
		ver = "6";
#endif /* INET6 */

#ifdef INET6
	if (req.dstaddr.ss_family == AF_INET6)
		in6_fillscopeid((struct sockaddr_in6 *)&req.dstaddr);
#endif /* INET6 */
	if (getnameinfo((struct sockaddr *)&req.dstaddr, req.dstaddr.ss_len,
	    pdstaddr, sizeof(pdstaddr), 0, 0, niflag) != 0)
		strlcpy(pdstaddr, "<error>", sizeof(pdstaddr));

	printf("\tphysical address inet%s %s --> %s", ver,
	    psrcaddr, pdstaddr);

	if (ioctl(s, SIOCGLIFPHYRTABLEID, (caddr_t)&ifr) == 0 &&
	    (rdomainid != 0 || ifr.ifr_rdomainid != 0))
		printf(" rdomain %d", ifr.ifr_rdomainid);
	printf("\n");
}

const int ifm_status_valid_list[] = IFM_STATUS_VALID_LIST;

const struct ifmedia_status_description ifm_status_descriptions[] =
	IFM_STATUS_DESCRIPTIONS;

const struct if_status_description if_status_descriptions[] =
	LINK_STATE_DESCRIPTIONS;

const char *
get_linkstate(int mt, int link_state)
{
	const struct if_status_description *p;
	static char buf[8];

	for (p = if_status_descriptions; p->ifs_string != NULL; p++) {
		if (LINK_STATE_DESC_MATCH(p, mt, link_state))
			return (p->ifs_string);
	}
	snprintf(buf, sizeof(buf), "[#%d]", link_state);
	return buf;
}

/*
 * Print the status of the interface.  If an address family was
 * specified, show it and it only; otherwise, show them all.
 */
void
status(int link, struct sockaddr_dl *sdl, int ls)
{
	const struct afswtch *p = afp;
	struct ifmediareq ifmr;
	struct ifreq ifrdesc;
	int *media_list, i;
	char ifdescr[IFDESCRSIZE];

	printf("%s: ", name);
	printb("flags", flags, IFFBITS);
	if (rdomainid)
		printf(" rdomain %i", rdomainid);
	if (metric)
		printf(" metric %lu", metric);
	if (mtu)
		printf(" mtu %lu", mtu);
	putchar('\n');
	if (sdl != NULL && sdl->sdl_alen &&
	    (sdl->sdl_type == IFT_ETHER || sdl->sdl_type == IFT_CARP))
		(void)printf("\tlladdr %s\n", ether_ntoa(
		    (struct ether_addr *)LLADDR(sdl)));

	(void) memset(&ifrdesc, 0, sizeof(ifrdesc));
	(void) strlcpy(ifrdesc.ifr_name, name, sizeof(ifrdesc.ifr_name));
	ifrdesc.ifr_data = (caddr_t)&ifdescr;
	if (ioctl(s, SIOCGIFDESCR, &ifrdesc) == 0 &&
	    strlen(ifrdesc.ifr_data))
		printf("\tdescription: %s\n", ifrdesc.ifr_data);

#ifndef SMALL
	if (!is_bridge(name) && ioctl(s, SIOCGIFPRIORITY, &ifrdesc) == 0)
		printf("\tpriority: %d\n", ifrdesc.ifr_metric);
#endif
	vlan_status();
#ifndef SMALL
	carp_status();
	pfsync_status();
	pppoe_status();
	timeslot_status();
	sppp_status();
	trunk_status();
	mpe_status();
	pflow_status();
#endif
	getifgroups();

	(void) memset(&ifmr, 0, sizeof(ifmr));
	(void) strlcpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));

	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
		/*
		 * Interface doesn't support SIOC{G,S}IFMEDIA.
		 */
		if (ls != LINK_STATE_UNKNOWN)
			printf("\tstatus: %s\n",
			    get_linkstate(sdl->sdl_type, ls));
		goto proto_status;
	}

	if (ifmr.ifm_count == 0) {
		warnx("%s: no media types?", name);
		goto proto_status;
	}

	media_list = (int *)calloc(ifmr.ifm_count, sizeof(int));
	if (media_list == NULL)
		err(1, "malloc");
	ifmr.ifm_ulist = media_list;

	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0)
		err(1, "SIOCGIFMEDIA");

	printf("\tmedia: ");
	print_media_word(ifmr.ifm_current, 1, 0);
	if (ifmr.ifm_active != ifmr.ifm_current) {
		putchar(' ');
		putchar('(');
		print_media_word(ifmr.ifm_active, 0, 0);
		putchar(')');
	}
	putchar('\n');

	if (ifmr.ifm_status & IFM_AVALID) {
		const struct ifmedia_status_description *ifms;
		int bitno, found = 0;

		printf("\tstatus: ");
		for (bitno = 0; ifm_status_valid_list[bitno] != 0; bitno++) {
			for (ifms = ifm_status_descriptions;
			    ifms->ifms_valid != 0; ifms++) {
				if (ifms->ifms_type !=
				    IFM_TYPE(ifmr.ifm_current) ||
				    ifms->ifms_valid !=
				    ifm_status_valid_list[bitno])
					continue;
				printf("%s%s", found ? ", " : "",
				    IFM_STATUS_DESC(ifms, ifmr.ifm_status));
				found = 1;

				/*
				 * For each valid indicator bit, there's
				 * only one entry for each media type, so
				 * terminate the inner loop now.
				 */
				break;
			}
		}

		if (found == 0)
			printf("unknown");
		putchar('\n');
	}

	ieee80211_status();

	if (showmediaflag) {
		int type, printed_type = 0;

		for (type = IFM_NMIN; type <= IFM_NMAX; type += IFM_NMIN) {
			for (i = 0, printed_type = 0; i < ifmr.ifm_count; i++) {
				if (IFM_TYPE(media_list[i]) == type) {
					if (printed_type == 0) {
					    printf("\tsupported media:\n");
					    printed_type = 1;
					}
					printf("\t\t");
					print_media_word(media_list[i], 0, 1);
					printf("\n");
				}
			}
		}
	}

	free(media_list);

 proto_status:
	if (link == 0) {
		if ((p = afp) != NULL) {
			(*p->af_status)(1);
		} else for (p = afs; p->af_name; p++) {
			ifr.ifr_addr.sa_family = p->af_af;
			(*p->af_status)(0);
		}
	}

	phys_status(0);
#ifndef SMALL
	bridge_status();
#endif
}

/* ARGSUSED */
void
in_status(int force)
{
	struct sockaddr_in *sin, sin2;

	getsock(AF_INET);
	if (s < 0) {
		if (errno == EPROTONOSUPPORT)
			return;
		err(1, "socket");
	}
	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	sin = (struct sockaddr_in *)&ifr.ifr_addr;

	/*
	 * We keep the interface address and reset it before each
	 * ioctl() so we can get ifaliases information (as opposed
	 * to the primary interface netmask/dstaddr/broadaddr, if
	 * the ifr_addr field is zero).
	 */
	memcpy(&sin2, &ifr.ifr_addr, sizeof(sin2));

	printf("\tinet %s", inet_ntoa(sin->sin_addr));
	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFNETMASK, (caddr_t)&ifr) < 0) {
		if (errno != EADDRNOTAVAIL)
			warn("SIOCGIFNETMASK");
		memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
	} else
		netmask.sin_addr =
		    ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
	if (flags & IFF_POINTOPOINT) {
		memcpy(&ifr.ifr_addr, &sin2, sizeof(sin2));
		if (ioctl(s, SIOCGIFDSTADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
			else
			    warn("SIOCGIFDSTADDR");
		}
		(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
		sin = (struct sockaddr_in *)&ifr.ifr_dstaddr;
		printf(" --> %s", inet_ntoa(sin->sin_addr));
	}
	printf(" netmask 0x%x", ntohl(netmask.sin_addr.s_addr));
	if (flags & IFF_BROADCAST) {
		memcpy(&ifr.ifr_addr, &sin2, sizeof(sin2));
		if (ioctl(s, SIOCGIFBRDADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
			else
			    warn("SIOCGIFBRDADDR");
		}
		(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
		sin = (struct sockaddr_in *)&ifr.ifr_addr;
		if (sin->sin_addr.s_addr != 0)
			printf(" broadcast %s", inet_ntoa(sin->sin_addr));
	}
	putchar('\n');
}

/* ARGSUSED */
void
setifprefixlen(const char *addr, int d)
{
	if (*afp->af_getprefix)
		(*afp->af_getprefix)(addr, MASK);
	explicit_prefix = 1;
}

#ifdef INET6
void
in6_fillscopeid(struct sockaddr_in6 *sin6)
{
#ifdef __KAME__
	if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
		sin6->sin6_scope_id =
			ntohs(*(u_int16_t *)&sin6->sin6_addr.s6_addr[2]);
		sin6->sin6_addr.s6_addr[2] = sin6->sin6_addr.s6_addr[3] = 0;
	}
#endif /* __KAME__ */
}

/* XXX not really an alias */
void
in6_alias(struct in6_ifreq *creq)
{
	struct sockaddr_in6 *sin6;
	struct	in6_ifreq ifr6;		/* shadows file static variable */
	u_int32_t scopeid;
	char hbuf[NI_MAXHOST];
	const int niflag = NI_NUMERICHOST;

	/* Get the non-alias address for this interface. */
	getsock(AF_INET6);
	if (s < 0) {
		if (errno == EPROTONOSUPPORT)
			return;
		err(1, "socket");
	}

	sin6 = (struct sockaddr_in6 *)&creq->ifr_addr;

	in6_fillscopeid(sin6);
	scopeid = sin6->sin6_scope_id;
	if (getnameinfo((struct sockaddr *)sin6, sin6->sin6_len,
	    hbuf, sizeof(hbuf), NULL, 0, niflag) != 0)
		strlcpy(hbuf, "", sizeof hbuf);
	printf("\tinet6 %s", hbuf);

	if (flags & IFF_POINTOPOINT) {
		(void) memset(&ifr6, 0, sizeof(ifr6));
		(void) strlcpy(ifr6.ifr_name, name, sizeof(ifr6.ifr_name));
		ifr6.ifr_addr = creq->ifr_addr;
		if (ioctl(s, SIOCGIFDSTADDR_IN6, (caddr_t)&ifr6) < 0) {
			if (errno != EADDRNOTAVAIL)
				warn("SIOCGIFDSTADDR_IN6");
			(void) memset(&ifr6.ifr_addr, 0, sizeof(ifr6.ifr_addr));
			ifr6.ifr_addr.sin6_family = AF_INET6;
			ifr6.ifr_addr.sin6_len = sizeof(struct sockaddr_in6);
		}
		sin6 = (struct sockaddr_in6 *)&ifr6.ifr_addr;
		in6_fillscopeid(sin6);
		if (getnameinfo((struct sockaddr *)sin6, sin6->sin6_len,
		    hbuf, sizeof(hbuf), NULL, 0, niflag) != 0)
			strlcpy(hbuf, "", sizeof hbuf);
		printf(" -> %s", hbuf);
	}

	(void) memset(&ifr6, 0, sizeof(ifr6));
	(void) strlcpy(ifr6.ifr_name, name, sizeof(ifr6.ifr_name));
	ifr6.ifr_addr = creq->ifr_addr;
	if (ioctl(s, SIOCGIFNETMASK_IN6, (caddr_t)&ifr6) < 0) {
		if (errno != EADDRNOTAVAIL)
			warn("SIOCGIFNETMASK_IN6");
	} else {
		sin6 = (struct sockaddr_in6 *)&ifr6.ifr_addr;
		printf(" prefixlen %d", prefix(&sin6->sin6_addr,
		    sizeof(struct in6_addr)));
	}

	(void) memset(&ifr6, 0, sizeof(ifr6));
	(void) strlcpy(ifr6.ifr_name, name, sizeof(ifr6.ifr_name));
	ifr6.ifr_addr = creq->ifr_addr;
	if (ioctl(s, SIOCGIFAFLAG_IN6, (caddr_t)&ifr6) < 0) {
		if (errno != EADDRNOTAVAIL)
			warn("SIOCGIFAFLAG_IN6");
	} else {
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_ANYCAST)
			printf(" anycast");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_TENTATIVE)
			printf(" tentative");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DUPLICATED)
			printf(" duplicated");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DETACHED)
			printf(" detached");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DEPRECATED)
			printf(" deprecated");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_AUTOCONF)
			printf(" autoconf");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_PRIVACY)
			printf(" autoconfprivacy");
	}

	if (scopeid)
		printf(" scopeid 0x%x", scopeid);

	if (Lflag) {
		struct in6_addrlifetime *lifetime;
		(void) memset(&ifr6, 0, sizeof(ifr6));
		(void) strlcpy(ifr6.ifr_name, name, sizeof(ifr6.ifr_name));
		ifr6.ifr_addr = creq->ifr_addr;
		lifetime = &ifr6.ifr_ifru.ifru_lifetime;
		if (ioctl(s, SIOCGIFALIFETIME_IN6, (caddr_t)&ifr6) < 0) {
			if (errno != EADDRNOTAVAIL)
				warn("SIOCGIFALIFETIME_IN6");
		} else if (lifetime->ia6t_preferred || lifetime->ia6t_expire) {
			time_t t = time(NULL);
			printf(" pltime ");
			if (lifetime->ia6t_preferred) {
				printf("%s", lifetime->ia6t_preferred < t
				    ? "0" :
				    sec2str( lifetime->ia6t_preferred - t));
			} else
				printf("infty");

			printf(" vltime ");
			if (lifetime->ia6t_expire) {
				printf("%s", lifetime->ia6t_expire < t
				    ? "0"
				    : sec2str(lifetime->ia6t_expire - t));
			} else
				printf("infty");
		}
	}

	printf("\n");
}

void
in6_status(int force)
{
	in6_alias((struct in6_ifreq *)&ifr6);
}
#endif /*INET6*/

#ifndef SMALL
void
settunnel(const char *src, const char *dst)
{
	struct addrinfo hints, *srcres, *dstres;
	int ecode;
	struct if_laddrreq req;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = afp->af_af;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/

	if ((ecode = getaddrinfo(src, NULL, &hints, &srcres)) != 0)
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if ((ecode = getaddrinfo(dst, NULL, &hints, &dstres)) != 0)
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if (srcres->ai_addr->sa_family != dstres->ai_addr->sa_family)
		errx(1,
		    "source and destination address families do not match");

	if (srcres->ai_addrlen > sizeof(req.addr) ||
	    dstres->ai_addrlen > sizeof(req.dstaddr))
		errx(1, "invalid sockaddr");

	memset(&req, 0, sizeof(req));
	(void) strlcpy(req.iflr_name, name, sizeof(req.iflr_name));
	memcpy(&req.addr, srcres->ai_addr, srcres->ai_addrlen);
	memcpy(&req.dstaddr, dstres->ai_addr, dstres->ai_addrlen);
	if (ioctl(s, SIOCSLIFPHYADDR, &req) < 0)
		warn("SIOCSLIFPHYADDR");

	freeaddrinfo(srcres);
	freeaddrinfo(dstres);
}

/* ARGSUSED */
void
deletetunnel(const char *ignored, int alsoignored)
{
	if (ioctl(s, SIOCDIFPHYADDR, &ifr) < 0)
		warn("SIOCDIFPHYADDR");
}

void
settunnelinst(const char *id, int param)
{
	const char *errmsg = NULL;
	int rdomainid;

	rdomainid = strtonum(id, 0, 128, &errmsg);
	if (errmsg)
		errx(1, "rdomain %s: %s", id, errmsg);

	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_rdomainid = rdomainid;
	if (ioctl(s, SIOCSLIFPHYRTABLEID, (caddr_t)&ifr) < 0)
		warn("SIOCSLIFPHYRTABLEID");
}

void
at_status(int force)
{
	struct sockaddr_at *sat, null_sat;
	struct netrange *nr;

	getsock(AF_APPLETALK);
	if (s < 0) {
		if (errno == EPROTONOSUPPORT)
			return;
		err(1, "socket");
	}
	(void) memset(&ifr, 0, sizeof(ifr));
	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			if (!force)
				return;
			(void) memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
		} else
			warn("SIOCGIFADDR");
	}
	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	sat = (struct sockaddr_at *)&ifr.ifr_addr;

	(void) memset(&null_sat, 0, sizeof(null_sat));

	nr = (struct netrange *) &sat->sat_zero;
	printf("\tAppleTalk %d.%d range %d-%d phase %d",
	    ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node,
	    ntohs(nr->nr_firstnet), ntohs(nr->nr_lastnet), nr->nr_phase);
	if (flags & IFF_POINTOPOINT) {
		if (ioctl(s, SIOCGIFDSTADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    (void) memset(&ifr.ifr_addr, 0,
				sizeof(ifr.ifr_addr));
			else
			    warn("SIOCGIFDSTADDR");
		}
		(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
		sat = (struct sockaddr_at *)&ifr.ifr_dstaddr;
		if (!sat)
			sat = &null_sat;
		printf("--> %d.%d",
		    ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node);
	}
	if (flags & IFF_BROADCAST) {
		/* note RTAX_BRD overlap with IFF_POINTOPOINT */
		sat = (struct sockaddr_at *)&ifr.ifr_broadaddr;
		if (sat)
			printf(" broadcast %d.%d", ntohs(sat->sat_addr.s_net),
			    sat->sat_addr.s_node);
	}
	putchar('\n');
}

void
at_getaddr(const char *addr, int which)
{
	struct sockaddr_at *sat = (struct sockaddr_at *) &addreq.ifra_addr;
	u_int net, node;

	sat->sat_family = AF_APPLETALK;
	sat->sat_len = sizeof(*sat);
	if (which == MASK)
		errx(1, "AppleTalk does not use netmasks");
	if (sscanf(addr, "%u.%u", &net, &node) != 2 ||
	    net == 0 || net > 0xffff || node == 0 || node > 0xfe)
		errx(1, "%s: illegal address", addr);
	sat->sat_addr.s_net = htons(net);
	sat->sat_addr.s_node = node;
}

/* ARGSUSED */
void
setatrange(const char *range, int d)
{
	u_int first = 123, last = 123;

	if (sscanf(range, "%u-%u", &first, &last) != 2 ||
	    first == 0 || first > 0xffff ||
	    last == 0 || last > 0xffff || first > last)
		errx(1, "%s: illegal net range: %u-%u", range, first, last);
	at_nr.nr_firstnet = htons(first);
	at_nr.nr_lastnet = htons(last);
}

/* ARGSUSED */
void
setatphase(const char *phase, int d)
{
	if (!strcmp(phase, "1"))
		at_nr.nr_phase = 1;
	else if (!strcmp(phase, "2"))
		at_nr.nr_phase = 2;
	else
		errx(1, "%s: illegal phase", phase);
}

void
checkatrange(struct sockaddr_at *sat)
{
	if (at_nr.nr_phase == 0)
		at_nr.nr_phase = 2;	/* Default phase 2 */
	if (at_nr.nr_firstnet == 0)	/* Default range of one */
		at_nr.nr_firstnet = at_nr.nr_lastnet = sat->sat_addr.s_net;
	printf("\tatalk %d.%d range %d-%d phase %d\n",
	ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node,
	ntohs(at_nr.nr_firstnet), ntohs(at_nr.nr_lastnet), at_nr.nr_phase);
	if ((u_short) ntohs(at_nr.nr_firstnet) >
	    (u_short) ntohs(sat->sat_addr.s_net) ||
	    (u_short) ntohs(at_nr.nr_lastnet) <
	    (u_short) ntohs(sat->sat_addr.s_net))
		errx(1, "AppleTalk address is not in range");
	*((struct netrange *) &sat->sat_zero) = at_nr;
}

void
mpe_status(void)
{
	struct shim_hdr	shim;

	bzero(&shim, sizeof(shim));
	ifr.ifr_data = (caddr_t)&shim;

	if (ioctl(s, SIOCGETLABEL , (caddr_t)&ifr) == -1)
		return;
	printf("\tmpls label: %d\n", shim.shim_label);
}

void
setmpelabel(const char *val, int d)
{
	struct shim_hdr	 shim;
	const char	*estr;

	bzero(&shim, sizeof(shim));
	ifr.ifr_data = (caddr_t)&shim;
	shim.shim_label = strtonum(val, 0, MPLS_LABEL_MAX, &estr);

	if (estr)
		errx(1, "mpls label %s is %s", val, estr);
	if (ioctl(s, SIOCSETLABEL, (caddr_t)&ifr) == -1)
		warn("SIOCSETLABEL");
}
#endif /* SMALL */

static int __tag = 0;
static int __have_tag = 0;

void
vlan_status(void)
{
	struct vlanreq vreq;
	struct vlanreq preq;

	bzero((char *)&vreq, sizeof(struct vlanreq));
	ifr.ifr_data = (caddr_t)&vreq;

	if (ioctl(s, SIOCGETVLAN, (caddr_t)&ifr) == -1)
		return;

	bzero(&preq, sizeof(struct vlanreq));
	ifr.ifr_data = (caddr_t)&preq;

	if (ioctl(s, SIOCGETVLANPRIO, (caddr_t)&ifr) == -1)
		return;

	if (vreq.vlr_tag || (vreq.vlr_parent[0] != '\0'))
		printf("\tvlan: %d priority: %d parent interface: %s\n",
		    vreq.vlr_tag, preq.vlr_tag, vreq.vlr_parent[0] == '\0' ?
		    "<none>" : vreq.vlr_parent);
}

/* ARGSUSED */
void
setvlantag(const char *val, int d)
{
	u_int16_t tag;
	struct vlanreq vreq;
	const char *errmsg = NULL;

	__tag = tag = strtonum(val, 0, 65535, &errmsg);
	if (errmsg)
		errx(1, "vlan tag %s: %s", val, errmsg);
	__have_tag = 1;

	bzero((char *)&vreq, sizeof(struct vlanreq));
	ifr.ifr_data = (caddr_t)&vreq;

	if (ioctl(s, SIOCGETVLAN, (caddr_t)&ifr) == -1)
		err(1, "SIOCGETVLAN");

	vreq.vlr_tag = tag;

	if (ioctl(s, SIOCSETVLAN, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETVLAN");
}

/* ARGSUSED */
void
setvlanprio(const char *val, int d)
{
	u_int16_t prio;
	struct vlanreq vreq;
	const char *errmsg = NULL;

	prio = strtonum(val, 0, 7, &errmsg);
	if (errmsg)
		errx(1, "vlan priority %s: %s", val, errmsg);

	bzero(&vreq, sizeof(struct vlanreq));
	ifr.ifr_data = (caddr_t)&vreq;

	if (ioctl(s, SIOCGETVLANPRIO, (caddr_t)&ifr) == -1)
		err(1, "SIOCGETVLANPRIO");

	vreq.vlr_tag = prio;

	if (ioctl(s, SIOCSETVLANPRIO, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETVLANPRIO");
}

/* ARGSUSED */
void
setvlandev(const char *val, int d)
{
	struct vlanreq	 vreq;
	int		 tag;
	size_t		 skip;
	const char	*estr;

	bzero((char *)&vreq, sizeof(struct vlanreq));
	ifr.ifr_data = (caddr_t)&vreq;

	if (ioctl(s, SIOCGETVLAN, (caddr_t)&ifr) == -1)
		err(1, "SIOCGETVLAN");

	(void) strlcpy(vreq.vlr_parent, val, sizeof(vreq.vlr_parent));

	if (!__have_tag && vreq.vlr_tag == 0) {
		skip = strcspn(ifr.ifr_name, "0123456789");
		tag = strtonum(ifr.ifr_name + skip, 1, 4095, &estr);
		if (estr != NULL)
			errx(1, "invalid vlan tag and device specification");
		vreq.vlr_tag = tag;
	} else if (__have_tag)
		vreq.vlr_tag = __tag;

	if (ioctl(s, SIOCSETVLAN, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETVLAN");
}

/* ARGSUSED */
void
unsetvlandev(const char *val, int d)
{
	struct vlanreq vreq;

	bzero((char *)&vreq, sizeof(struct vlanreq));
	ifr.ifr_data = (caddr_t)&vreq;

	if (ioctl(s, SIOCGETVLAN, (caddr_t)&ifr) == -1)
		err(1, "SIOCGETVLAN");

	bzero((char *)&vreq.vlr_parent, sizeof(vreq.vlr_parent));
	vreq.vlr_tag = 0;

	if (ioctl(s, SIOCSETVLAN, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETVLAN");
}

#ifndef SMALL
static const char *carp_states[] = { CARP_STATES };
static const char *carp_bal_modes[] = { CARP_BAL_MODES };

void
carp_status(void)
{
	const char *state, *balmode;
	struct carpreq carpr;
	char peer[32];
	int i;

	memset((char *)&carpr, 0, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		return;

	if (carpr.carpr_vhids[0] == 0)
		return;

	if (carpr.carpr_balancing > CARP_BAL_MAXID)
		balmode = "<UNKNOWN>";
	else
		balmode = carp_bal_modes[carpr.carpr_balancing];

	if (carpr.carpr_peer.s_addr != htonl(INADDR_CARP_GROUP))
		snprintf(peer, sizeof(peer),
		    " carppeer %s", inet_ntoa(carpr.carpr_peer));
	else
		peer[0] = '\0';

	for (i = 0; carpr.carpr_vhids[i]; i++) {
		if (carpr.carpr_states[i] > CARP_MAXSTATE)
			state = "<UNKNOWN>";
		else
			state = carp_states[carpr.carpr_states[i]];
		if (carpr.carpr_vhids[1] == 0) {
			printf("\tcarp: %s carpdev %s vhid %u advbase %d "
			    "advskew %u%s\n", state,
			    carpr.carpr_carpdev[0] != '\0' ?
		    	    carpr.carpr_carpdev : "none", carpr.carpr_vhids[0],
		    	    carpr.carpr_advbase, carpr.carpr_advskews[0],
			    peer);
		} else {
			if (i == 0) {
				printf("\tcarp: carpdev %s advbase %d"
				    " balancing %s%s\n",
				    carpr.carpr_carpdev[0] != '\0' ?
				    carpr.carpr_carpdev : "none",
				    carpr.carpr_advbase, balmode, peer);
			}
			printf("\t\tstate %s vhid %u advskew %u\n", state,
			    carpr.carpr_vhids[i], carpr.carpr_advskews[i]);
		}
	}
}

/* ARGSUSED */
void
setcarp_passwd(const char *val, int d)
{
	struct carpreq carpr;

	memset((char *)&carpr, 0, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	/* XXX Should hash the password into the key here, perhaps? */
	strlcpy((char *)carpr.carpr_key, val, CARP_KEY_LEN);

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");
}

/* ARGSUSED */
void
setcarp_vhid(const char *val, int d)
{
	const char *errmsg = NULL;
	struct carpreq carpr;
	int vhid;

	vhid = strtonum(val, 1, 255, &errmsg);
	if (errmsg)
		errx(1, "vhid %s: %s", val, errmsg);

	memset((char *)&carpr, 0, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	carpr.carpr_vhids[0] = vhid;
	carpr.carpr_vhids[1] = 0;

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");
}

/* ARGSUSED */
void
setcarp_advskew(const char *val, int d)
{
	const char *errmsg = NULL;
	struct carpreq carpr;
	int advskew;

	advskew = strtonum(val, 0, 255, &errmsg);
	if (errmsg)
		errx(1, "advskew %s: %s", val, errmsg);

	memset((char *)&carpr, 0, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	carpr.carpr_advskews[0] = advskew;

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");
}

/* ARGSUSED */
void
setcarp_advbase(const char *val, int d)
{
	const char *errmsg = NULL;
	struct carpreq carpr;
	int advbase;

	advbase = strtonum(val, 0, 255, &errmsg);
	if (errmsg)
		errx(1, "advbase %s: %s", val, errmsg);

	memset((char *)&carpr, 0, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	carpr.carpr_advbase = advbase;

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");
}

/* ARGSUSED */
void
setcarppeer(const char *val, int d)
{
	struct carpreq carpr;
	struct addrinfo hints, *peerres;
	int ecode;

	memset((char *)&carpr, 0, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((ecode = getaddrinfo(val, NULL, &hints, &peerres)) != 0)
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if (peerres->ai_addr->sa_family != AF_INET)
		errx(1, "only IPv4 addresses supported for the carppeer");

	carpr.carpr_peer.s_addr = ((struct sockaddr_in *)
	    peerres->ai_addr)->sin_addr.s_addr;

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");

	freeaddrinfo(peerres);
}

void
unsetcarppeer(const char *val, int d)
{
	struct carpreq carpr;

	bzero((char *)&carpr, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	bzero((char *)&carpr.carpr_peer, sizeof(carpr.carpr_peer));

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");
}

/* ARGSUSED */
void
setcarp_state(const char *val, int d)
{
	struct carpreq carpr;
	int i;

	bzero((char *)&carpr, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	for (i = 0; i <= CARP_MAXSTATE; i++) {
		if (!strcasecmp(val, carp_states[i])) {
			carpr.carpr_state = i;
			break;
		}
	}

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");
}

/* ARGSUSED */
void
setcarpdev(const char *val, int d)
{
	struct carpreq carpr;

	bzero((char *)&carpr, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	strlcpy(carpr.carpr_carpdev, val, sizeof(carpr.carpr_carpdev));

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");
}

void
unsetcarpdev(const char *val, int d)
{
	struct carpreq carpr;

	bzero((char *)&carpr, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	bzero((char *)&carpr.carpr_carpdev, sizeof(carpr.carpr_carpdev));

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");
}

void
setcarp_nodes(const char *val, int d)
{
	char *optlist, *str;
	int i;
	struct carpreq carpr;

	bzero((char *)&carpr, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	bzero(carpr.carpr_vhids, sizeof(carpr.carpr_vhids));
	bzero(carpr.carpr_advskews, sizeof(carpr.carpr_advskews));

	optlist = strdup(val);
	if (optlist == NULL)
		err(1, "strdup");

	str = strtok(optlist, ",");
	for (i = 0; str != NULL; i++) {
		u_int vhid, advskew;

		if (i >= CARP_MAXNODES)
			errx(1, "too many carp nodes");
		if (sscanf(str, "%u:%u", &vhid, &advskew) != 2) {
			errx(1, "non parsable arg: %s", str);
		}
		if (vhid >= 255)
			errx(1, "vhid %u: value too large", vhid);
		if (advskew >= 255)
			errx(1, "advskew %u: value too large", advskew);

		carpr.carpr_vhids[i] = vhid;
		carpr.carpr_advskews[i] = advskew;
		str = strtok(NULL, ",");
	}
	free(optlist);

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");
}

void
setcarp_balancing(const char *val, int d)
{
	int i;
	struct carpreq carpr;

	bzero((char *)&carpr, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	for (i = 0; i <= CARP_BAL_MAXID; i++)
		if (!strcasecmp(val, carp_bal_modes[i]))
			break;

	if (i > CARP_BAL_MAXID)
		errx(1, "balancing %s: unknown mode", val);

	carpr.carpr_balancing = i;

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");
}

void
setpfsync_syncdev(const char *val, int d)
{
	struct pfsyncreq preq;

	bzero((char *)&preq, sizeof(struct pfsyncreq));
	ifr.ifr_data = (caddr_t)&preq;

	if (ioctl(s, SIOCGETPFSYNC, (caddr_t)&ifr) == -1)
		err(1, "SIOCGETPFSYNC");

	strlcpy(preq.pfsyncr_syncdev, val, sizeof(preq.pfsyncr_syncdev));

	if (ioctl(s, SIOCSETPFSYNC, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFSYNC");
}

/* ARGSUSED */
void
unsetpfsync_syncdev(const char *val, int d)
{
	struct pfsyncreq preq;

	bzero((char *)&preq, sizeof(struct pfsyncreq));
	ifr.ifr_data = (caddr_t)&preq;

	if (ioctl(s, SIOCGETPFSYNC, (caddr_t)&ifr) == -1)
		err(1, "SIOCGETPFSYNC");

	bzero((char *)&preq.pfsyncr_syncdev, sizeof(preq.pfsyncr_syncdev));

	if (ioctl(s, SIOCSETPFSYNC, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFSYNC");
}

/* ARGSUSED */
void
setpfsync_syncpeer(const char *val, int d)
{
	struct pfsyncreq preq;
	struct addrinfo hints, *peerres;
	int ecode;

	bzero((char *)&preq, sizeof(struct pfsyncreq));
	ifr.ifr_data = (caddr_t)&preq;

	if (ioctl(s, SIOCGETPFSYNC, (caddr_t)&ifr) == -1)
		err(1, "SIOCGETPFSYNC");

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/

	if ((ecode = getaddrinfo(val, NULL, &hints, &peerres)) != 0)
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if (peerres->ai_addr->sa_family != AF_INET)
		errx(1, "only IPv4 addresses supported for the syncpeer");

	preq.pfsyncr_syncpeer.s_addr = ((struct sockaddr_in *)
	    peerres->ai_addr)->sin_addr.s_addr;

	if (ioctl(s, SIOCSETPFSYNC, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFSYNC");

	freeaddrinfo(peerres);
}

/* ARGSUSED */
void
unsetpfsync_syncpeer(const char *val, int d)
{
	struct pfsyncreq preq;

	bzero((char *)&preq, sizeof(struct pfsyncreq));
	ifr.ifr_data = (caddr_t)&preq;

	if (ioctl(s, SIOCGETPFSYNC, (caddr_t)&ifr) == -1)
		err(1, "SIOCGETPFSYNC");

	preq.pfsyncr_syncpeer.s_addr = 0;

	if (ioctl(s, SIOCSETPFSYNC, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFSYNC");
}

/* ARGSUSED */
void
setpfsync_maxupd(const char *val, int d)
{
	const char *errmsg = NULL;
	struct pfsyncreq preq;
	int maxupdates;

	maxupdates = strtonum(val, 0, 255, &errmsg);
	if (errmsg)
		errx(1, "maxupd %s: %s", val, errmsg);

	memset((char *)&preq, 0, sizeof(struct pfsyncreq));
	ifr.ifr_data = (caddr_t)&preq;

	if (ioctl(s, SIOCGETPFSYNC, (caddr_t)&ifr) == -1)
		err(1, "SIOCGETPFSYNC");

	preq.pfsyncr_maxupdates = maxupdates;

	if (ioctl(s, SIOCSETPFSYNC, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFSYNC");
}

void
setpfsync_defer(const char *val, int d)
{
	struct pfsyncreq preq;

	memset((char *)&preq, 0, sizeof(struct pfsyncreq));
	ifr.ifr_data = (caddr_t)&preq;

	if (ioctl(s, SIOCGETPFSYNC, (caddr_t)&ifr) == -1)
		err(1, "SIOCGETPFSYNC");

	preq.pfsyncr_defer = d;
	if (ioctl(s, SIOCSETPFSYNC, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFSYNC");
}

void
pfsync_status(void)
{
	struct pfsyncreq preq;

	bzero((char *)&preq, sizeof(struct pfsyncreq));
	ifr.ifr_data = (caddr_t)&preq;

	if (ioctl(s, SIOCGETPFSYNC, (caddr_t)&ifr) == -1)
		return;

	if (preq.pfsyncr_syncdev[0] != '\0') {
		printf("\tpfsync: syncdev: %s ", preq.pfsyncr_syncdev);
		if (preq.pfsyncr_syncpeer.s_addr != htonl(INADDR_PFSYNC_GROUP))
			printf("syncpeer: %s ",
			    inet_ntoa(preq.pfsyncr_syncpeer));
		printf("maxupd: %d ", preq.pfsyncr_maxupdates);
		printf("defer: %s\n", preq.pfsyncr_defer ? "on" : "off");
	}
}

#ifndef SMALL
void
pflow_status(void)
{
	struct pflowreq preq;

	bzero((char *)&preq, sizeof(struct pflowreq));
	ifr.ifr_data = (caddr_t)&preq;

	if (ioctl(s, SIOCGETPFLOW, (caddr_t)&ifr) == -1)
		 return;

	printf("\tpflow: sender: %s ", inet_ntoa(preq.sender_ip));
	printf("receiver: %s:%u\n", inet_ntoa(preq.receiver_ip),
	    ntohs(preq.receiver_port));
}

/* ARGSUSED */
void
setpflow_sender(const char *val, int d)
{
	struct pflowreq preq;
	struct addrinfo hints, *sender;
	int ecode;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM; /*dummy*/

	if ((ecode = getaddrinfo(val, NULL, &hints, &sender)) != 0)
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if (sender->ai_addr->sa_family != AF_INET)
		errx(1, "only IPv4 addresses supported for the sender");

	bzero((char *)&preq, sizeof(struct pflowreq));
	ifr.ifr_data = (caddr_t)&preq;
	preq.addrmask |= PFLOW_MASK_SRCIP;
	preq.sender_ip.s_addr = ((struct sockaddr_in *)
	    sender->ai_addr)->sin_addr.s_addr;
	

	if (ioctl(s, SIOCSETPFLOW, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFLOW");

	freeaddrinfo(sender);
}

void
unsetpflow_sender(const char *val, int d)
{
	struct pflowreq preq;

	bzero((char *)&preq, sizeof(struct pflowreq));
	preq.addrmask |= PFLOW_MASK_SRCIP;
	ifr.ifr_data = (caddr_t)&preq;
	if (ioctl(s, SIOCSETPFLOW, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFLOW");
}

/* ARGSUSED */
void
setpflow_receiver(const char *val, int d)
{
	struct pflowreq preq;
	struct addrinfo hints, *receiver;
	int ecode;
	char *ip, *port, buf[MAXHOSTNAMELEN+sizeof (":65535")];

	if (strchr (val, ':') == NULL)
		errx(1, "%s bad value", val);

	if (strlcpy(buf, val, sizeof(buf)) >= sizeof(buf))
		errx(1, "%s bad value", val);
	port = strchr(buf, ':');
	*port++ = '\0';
	ip = buf;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM; /*dummy*/

	if ((ecode = getaddrinfo(ip, port, &hints, &receiver)) != 0)
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if (receiver->ai_addr->sa_family != AF_INET)
		errx(1, "only IPv4 addresses supported for the receiver");

	bzero((char *)&preq, sizeof(struct pflowreq));
	ifr.ifr_data = (caddr_t)&preq;
	preq.addrmask |= PFLOW_MASK_DSTIP | PFLOW_MASK_DSTPRT;
	preq.receiver_ip.s_addr = ((struct sockaddr_in *)
	    receiver->ai_addr)->sin_addr.s_addr;
	preq.receiver_port = (u_int16_t) ((struct sockaddr_in *)
	    receiver->ai_addr)->sin_port;

	if (ioctl(s, SIOCSETPFLOW, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFLOW");

	freeaddrinfo(receiver);
}

void
unsetpflow_receiver(const char *val, int d)
{
	struct pflowreq preq;

	bzero((char *)&preq, sizeof(struct pflowreq));
	ifr.ifr_data = (caddr_t)&preq;
	preq.addrmask |= PFLOW_MASK_DSTIP | PFLOW_MASK_DSTPRT;
	if (ioctl(s, SIOCSETPFLOW, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFLOW");
}
#endif /* SMALL */

void
pppoe_status(void)
{
	struct pppoediscparms parms;
	struct pppoeconnectionstate state;
	struct timeval temp_time;
	long diff_time;
	unsigned long day, hour, min, sec;

	day = hour = min = sec = 0; /* XXX make gcc happy */

	memset(&state, 0, sizeof(state));
	memset(&temp_time, 0, sizeof(temp_time));

	strlcpy(parms.ifname, name, sizeof(parms.ifname));
	if (ioctl(s, PPPOEGETPARMS, &parms))
		return;

	printf("\tdev: %s ", parms.eth_ifname);

	if (*parms.ac_name)
		printf("ac: %s ", parms.ac_name);
	if (*parms.service_name)
		printf("svc: %s ", parms.service_name);

	strlcpy(state.ifname, name, sizeof(state.ifname));
	if (ioctl(s, PPPOEGETSESSION, &state))
		err(1, "PPPOEGETSESSION");

	printf("state: ");
	switch (state.state) {
	case PPPOE_STATE_INITIAL:
		printf("initial"); break;
	case PPPOE_STATE_PADI_SENT:
		printf("PADI sent"); break;
	case PPPOE_STATE_PADR_SENT:
		printf("PADR sent"); break;
	case PPPOE_STATE_SESSION:
		printf("session"); break;
	case PPPOE_STATE_CLOSING:
		printf("closing"); break;
	}
	printf("\n\tsid: 0x%x", state.session_id);
	printf(" PADI retries: %d", state.padi_retry_no);
	printf(" PADR retries: %d", state.padr_retry_no);

	if (state.state == PPPOE_STATE_SESSION) {
		if (state.session_time.tv_sec != 0) {
			gettimeofday(&temp_time, NULL);
			diff_time = temp_time.tv_sec -
			    state.session_time.tv_sec;

			day = diff_time / (60 * 60 * 24);
			diff_time %= (60 * 60 * 24);

			hour = diff_time / (60 * 60);
			diff_time %= (60 * 60);

			min = diff_time / 60;
			diff_time %= 60;

			sec = diff_time;
		}
		printf(" time: ");
		if (day != 0) printf("%ldd ", day);
		printf("%02ld:%02ld:%02ld", hour, min, sec);
	}
	putchar('\n');
}

/* ARGSUSED */
void
setpppoe_dev(const char *val, int d)
{
	struct pppoediscparms parms;

	strlcpy(parms.ifname, name, sizeof(parms.ifname));
	if (ioctl(s, PPPOEGETPARMS, &parms))
		return;

	strlcpy(parms.eth_ifname, val, sizeof(parms.eth_ifname));

	if (ioctl(s, PPPOESETPARMS, &parms))
		err(1, "PPPOESETPARMS");
}

/* ARGSUSED */
void
setpppoe_svc(const char *val, int d)
{
	struct pppoediscparms parms;

	strlcpy(parms.ifname, name, sizeof(parms.ifname));
	if (ioctl(s, PPPOEGETPARMS, &parms))
		return;

	if (d == 0)
		strlcpy(parms.service_name, val, sizeof(parms.service_name));
	else
		memset(parms.service_name, 0, sizeof(parms.service_name));

	if (ioctl(s, PPPOESETPARMS, &parms))
		err(1, "PPPOESETPARMS");
}

/* ARGSUSED */
void
setpppoe_ac(const char *val, int d)
{
	struct pppoediscparms parms;

	strlcpy(parms.ifname, name, sizeof(parms.ifname));
	if (ioctl(s, PPPOEGETPARMS, &parms))
		return;

	if (d == 0)
		strlcpy(parms.ac_name, val, sizeof(parms.ac_name));
	else
		memset(parms.ac_name, 0, sizeof(parms.ac_name));

	if (ioctl(s, PPPOESETPARMS, &parms))
		err(1, "PPPOESETPARMS");
}

void
spppinfo(struct spppreq *spr)
{
	bzero(spr, sizeof(struct spppreq));

	ifr.ifr_data = (caddr_t)spr;
	spr->cmd = SPPPIOGDEFS;
	if (ioctl(s, SIOCGIFGENERIC, &ifr) == -1)
		err(1, "SIOCGIFGENERIC(SPPPIOGDEFS)");
}

void
spppauthinfo(struct sauthreq *spa, int d)
{
	bzero(spa, sizeof(struct sauthreq));

	ifr.ifr_data = (caddr_t)spa;
	spa->cmd = d == 0 ? SPPPIOGMAUTH : SPPPIOGHAUTH;
	if (ioctl(s, SIOCGIFGENERIC, &ifr) == -1)
		err(1, "SIOCGIFGENERIC(SPPPIOGXAUTH)");
}

void
setspppproto(const char *val, int d)
{
	struct sauthreq spa;

	spppauthinfo(&spa, d);

	if (strcmp(val, "pap") == 0)
		spa.proto = PPP_PAP;
	else if (strcmp(val, "chap") == 0)
		spa.proto = PPP_CHAP;
	else if (strcmp(val, "none") == 0)
		spa.proto = 0;
	else
		errx(1, "setpppproto");

	spa.cmd = d == 0 ? SPPPIOSMAUTH : SPPPIOSHAUTH;
	if (ioctl(s, SIOCSIFGENERIC, &ifr) == -1)
		err(1, "SIOCSIFGENERIC(SPPPIOSXAUTH)");
}

void
setsppppeerproto(const char *val, int d)
{
	setspppproto(val, 1);
}

void
setspppname(const char *val, int d)
{
	struct sauthreq spa;

	spppauthinfo(&spa, d);

	if (spa.proto == 0)
		errx(1, "unspecified protocol");
	if (strlcpy(spa.name, val, sizeof(spa.name)) >= sizeof(spa.name))
		errx(1, "setspppname");

	spa.cmd = d == 0 ? SPPPIOSMAUTH : SPPPIOSHAUTH;
	if (ioctl(s, SIOCSIFGENERIC, &ifr) == -1)
		err(1, "SIOCSIFGENERIC(SPPPIOSXAUTH)");
}

void
setsppppeername(const char *val, int d)
{
	setspppname(val, 1);
}

void
setspppkey(const char *val, int d)
{
	struct sauthreq spa;

	spppauthinfo(&spa, d);

	if (spa.proto == 0)
		errx(1, "unspecified protocol");
	if (strlcpy(spa.secret, val, sizeof(spa.secret)) >= sizeof(spa.secret))
		errx(1, "setspppkey");

	spa.cmd = d == 0 ? SPPPIOSMAUTH : SPPPIOSHAUTH;
	if (ioctl(s, SIOCSIFGENERIC, &ifr) == -1)
		err(1, "SIOCSIFGENERIC(SPPPIOSXAUTH)");
}

void
setsppppeerkey(const char *val, int d)
{
	setspppkey(val, 1);
}

void
setsppppeerflag(const char *val, int d)
{
	struct sauthreq spa;
	int flag;

	spppauthinfo(&spa, 1);

	if (spa.proto == 0)
		errx(1, "unspecified protocol");
	if (strcmp(val, "callin") == 0)
		flag = AUTHFLAG_NOCALLOUT;
	else if (strcmp(val, "norechallenge") == 0)
		flag = AUTHFLAG_NORECHALLENGE;
	else
		errx(1, "setppppeerflags");

	if (d)
		spa.flags &= ~flag;
	else
		spa.flags |= flag;

	spa.cmd = SPPPIOSHAUTH;
	if (ioctl(s, SIOCSIFGENERIC, &ifr) == -1)
		err(1, "SIOCSIFGENERIC(SPPPIOSXAUTH)");
}

void
unsetsppppeerflag(const char *val, int d)
{
	setsppppeerflag(val, 1);
}

void
sppp_printproto(const char *name, struct sauthreq *auth)
{
	if (auth->proto == 0)
		return;
	printf("%sproto ", name);
	switch (auth->proto) {
	case PPP_PAP:
		printf("pap ");
		break;
	case PPP_CHAP:
		printf("chap ");
		break;
	default:
		printf("0x%04x ", auth->proto);
		break;
	}
	if (auth->name[0])
		printf("%sname \"%s\" ", name, auth->name);
	if (auth->secret[0])
		printf("%skey \"%s\" ", name, auth->secret);
}

void
sppp_status(void)
{
	struct spppreq spr;
	struct sauthreq spa;

	bzero(&spr, sizeof(spr));

	ifr.ifr_data = (caddr_t)&spr;
	spr.cmd = SPPPIOGDEFS;
	if (ioctl(s, SIOCGIFGENERIC, &ifr) == -1) {
		return;
	}

	if (spr.defs.pp_phase == PHASE_DEAD)
		return;
	printf("\tsppp: phase ");
	switch (spr.defs.pp_phase) {
	case PHASE_ESTABLISH:
		printf("establish ");
		break;
	case PHASE_TERMINATE:
		printf("terminate ");
		break;
	case PHASE_AUTHENTICATE:
		printf("authenticate ");
		break;
	case PHASE_NETWORK:
		printf("network ");
		break;
	default:
		printf("illegal ");
		break;
	}

	spppauthinfo(&spa, 0);
	sppp_printproto("auth", &spa);
	spppauthinfo(&spa, 1);
	sppp_printproto("peer", &spa);
	if (spa.flags & AUTHFLAG_NOCALLOUT)
		printf("callin ");
	if (spa.flags & AUTHFLAG_NORECHALLENGE)
		printf("norechallenge ");
	putchar('\n');
}

void
settrunkport(const char *val, int d)
{
	struct trunk_reqport rp;

	bzero(&rp, sizeof(rp));
	strlcpy(rp.rp_ifname, name, sizeof(rp.rp_ifname));
	strlcpy(rp.rp_portname, val, sizeof(rp.rp_portname));

	if (ioctl(s, SIOCSTRUNKPORT, &rp))
		err(1, "SIOCSTRUNKPORT");
}

void
unsettrunkport(const char *val, int d)
{
	struct trunk_reqport rp;

	bzero(&rp, sizeof(rp));
	strlcpy(rp.rp_ifname, name, sizeof(rp.rp_ifname));
	strlcpy(rp.rp_portname, val, sizeof(rp.rp_portname));

	if (ioctl(s, SIOCSTRUNKDELPORT, &rp))
		err(1, "SIOCSTRUNKDELPORT");
}

void
settrunkproto(const char *val, int d)
{
	struct trunk_protos tpr[] = TRUNK_PROTOS;
	struct trunk_reqall ra;
	int i;

	bzero(&ra, sizeof(ra));
	ra.ra_proto = TRUNK_PROTO_MAX;

	for (i = 0; i < (sizeof(tpr) / sizeof(tpr[0])); i++) {
		if (strcmp(val, tpr[i].tpr_name) == 0) {
			ra.ra_proto = tpr[i].tpr_proto;
			break;
		}
	}
	if (ra.ra_proto == TRUNK_PROTO_MAX)
		errx(1, "Invalid trunk protocol: %s", val);

	strlcpy(ra.ra_ifname, name, sizeof(ra.ra_ifname));
	if (ioctl(s, SIOCSTRUNK, &ra) != 0)
		err(1, "SIOCSTRUNK");
}

void
trunk_status(void)
{
	struct trunk_protos tpr[] = TRUNK_PROTOS;
	struct trunk_reqport rp, rpbuf[TRUNK_MAX_PORTS];
	struct trunk_reqall ra;
	struct lacp_opreq *lp;
	const char *proto = "<unknown>";
	int i, isport = 0;

	bzero(&rp, sizeof(rp));
	bzero(&ra, sizeof(ra));

	strlcpy(rp.rp_ifname, name, sizeof(rp.rp_ifname));
	strlcpy(rp.rp_portname, name, sizeof(rp.rp_portname));

	if (ioctl(s, SIOCGTRUNKPORT, &rp) == 0)
		isport = 1;

	strlcpy(ra.ra_ifname, name, sizeof(ra.ra_ifname));
	ra.ra_size = sizeof(rpbuf);
	ra.ra_port = rpbuf;

	if (ioctl(s, SIOCGTRUNK, &ra) == 0) {
		lp = (struct lacp_opreq *)&ra.ra_lacpreq;

		for (i = 0; i < (sizeof(tpr) / sizeof(tpr[0])); i++) {
			if (ra.ra_proto == tpr[i].tpr_proto) {
				proto = tpr[i].tpr_name;
				break;
			}
		}

		printf("\ttrunk: trunkproto %s", proto);
		if (isport)
			printf(" trunkdev %s", rp.rp_ifname);
		putchar('\n');
		if (ra.ra_proto == TRUNK_PROTO_LACP) {
			char *act_mac = strdup(
			    ether_ntoa((struct ether_addr*)lp->actor_mac));
			if (act_mac == NULL)
				err(1, "strdup");
			printf("\ttrunk id: [(%04X,%s,%04X,%04X,%04X),\n"
			    "\t\t (%04X,%s,%04X,%04X,%04X)]\n",
			    lp->actor_prio, act_mac,
			    lp->actor_key, lp->actor_portprio, lp->actor_portno,
			    lp->partner_prio,
			    ether_ntoa((struct ether_addr*)lp->partner_mac),
			    lp->partner_key, lp->partner_portprio,
			    lp->partner_portno);
			free(act_mac);
		}

		for (i = 0; i < ra.ra_ports; i++) {
			printf("\t\ttrunkport %s ", rpbuf[i].rp_portname);
			printb_status(rpbuf[i].rp_flags, TRUNK_PORT_BITS);
			putchar('\n');
		}

		if (showmediaflag) {
			printf("\tsupported trunk protocols:\n");
			for (i = 0; i < (sizeof(tpr) / sizeof(tpr[0])); i++)
				printf("\t\ttrunkproto %s\n", tpr[i].tpr_name);
		}
	} else if (isport)
		printf("\ttrunk: trunkdev %s\n", rp.rp_ifname);
}
#endif /* SMALL */

void
setifpriority(const char *id, int param)
{
#ifndef SMALL
	const char *errmsg = NULL;
	int prio;

	prio = strtonum(id, 0, 15, &errmsg);
	if (errmsg)
		errx(1, "priority %s: %s", id, errmsg);

	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_metric = prio;
	if (ioctl(s, SIOCSIFPRIORITY, (caddr_t)&ifr) < 0)
		warn("SIOCSIFPRIORITY");
#endif
}

#define SIN(x) ((struct sockaddr_in *) &(x))
struct sockaddr_in *sintab[] = {
SIN(ridreq.ifr_addr), SIN(in_addreq.ifra_addr),
SIN(in_addreq.ifra_mask), SIN(in_addreq.ifra_broadaddr)};

void
in_getaddr(const char *s, int which)
{
	struct sockaddr_in *sin = sintab[which], tsin;
	struct hostent *hp;
	struct netent *np;
	int bits, l;
	char p[3];

	bzero(&tsin, sizeof(tsin));
	sin->sin_len = sizeof(*sin);
	if (which != MASK)
		sin->sin_family = AF_INET;

	if (which == ADDR && strrchr(s, '/') != NULL &&
	    (bits = inet_net_pton(AF_INET, s, &tsin.sin_addr,
	    sizeof(tsin.sin_addr))) != -1) {
		l = snprintf(p, sizeof(p), "%i", bits);
		if (l >= sizeof(p) || l == -1)
			errx(1, "%i: bad prefixlen", bits);
		in_getprefix(p, MASK);
		memcpy(&sin->sin_addr, &tsin.sin_addr, sizeof(sin->sin_addr));
	} else if (inet_aton(s, &sin->sin_addr) == 0) {
		if ((hp = gethostbyname(s)))
			memcpy(&sin->sin_addr, hp->h_addr, hp->h_length);
		else if ((np = getnetbyname(s)))
			sin->sin_addr = inet_makeaddr(np->n_net, INADDR_ANY);
		else
			errx(1, "%s: bad value", s);
	}
}

/* ARGSUSED */
void
in_getprefix(const char *plen, int which)
{
	struct sockaddr_in *sin = sintab[which];
	const char *errmsg = NULL;
	u_char *cp;
	int len;

	len = strtonum(plen, 0, 32, &errmsg);
	if (errmsg)
		errx(1, "prefix %s: %s", plen, errmsg);

	sin->sin_len = sizeof(*sin);
	if (which != MASK)
		sin->sin_family = AF_INET;
	if ((len == 0) || (len == 32)) {
		memset(&sin->sin_addr, 0xff, sizeof(struct in_addr));
		return;
	}
	memset((void *)&sin->sin_addr, 0x00, sizeof(sin->sin_addr));
	for (cp = (u_char *)&sin->sin_addr; len > 7; len -= 8)
		*cp++ = 0xff;
	if (len)
		*cp = 0xff << (8 - len);
}

/*
 * Print a value a la the %b format of the kernel's printf
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

	if (bits) {
		bits++;
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

/*
 * A simple version of printb for status output
 */
void
printb_status(unsigned short v, char *bits)
{
	int i, any = 0;
	char c;

	if (bits) {
		bits++;
		while ((i = *bits++)) {
			if (v & (1 << (i-1))) {
				if (any)
					putchar(',');
				any = 1;
				for (; (c = *bits) > 32; bits++)
					putchar(tolower(c));
			} else
				for (; *bits > 32; bits++)
					;
		}
	}
}

#ifdef INET6
#define SIN6(x) ((struct sockaddr_in6 *) &(x))
struct sockaddr_in6 *sin6tab[] = {
SIN6(in6_ridreq.ifr_addr), SIN6(in6_addreq.ifra_addr),
SIN6(in6_addreq.ifra_prefixmask), SIN6(in6_addreq.ifra_dstaddr)};

void
in6_getaddr(const char *s, int which)
{
	struct sockaddr_in6 *sin6 = sin6tab[which];
	struct addrinfo hints, *res;
	char buf[MAXHOSTNAMELEN+sizeof("/128")], *pfxlen;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/

	if (which == ADDR && strchr(s, '/') != NULL) {
		if (strlcpy(buf, s, sizeof(buf)) >= sizeof(buf))
			errx(1, "%s: bad value", s);
		pfxlen = strchr(buf, '/');
		*pfxlen++ = '\0';
		s = buf;
		in6_getprefix(pfxlen, MASK);
		explicit_prefix = 1;
	}

	error = getaddrinfo(s, "0", &hints, &res);
	if (error)
		errx(1, "%s: %s", s, gai_strerror(error));
	if (res->ai_addrlen != sizeof(struct sockaddr_in6))
		errx(1, "%s: bad value", s);
	memcpy(sin6, res->ai_addr, res->ai_addrlen);
#ifdef __KAME__
	if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) &&
	    *(u_int16_t *)&sin6->sin6_addr.s6_addr[2] == 0 &&
	    sin6->sin6_scope_id) {
		*(u_int16_t *)&sin6->sin6_addr.s6_addr[2] =
		    htons(sin6->sin6_scope_id & 0xffff);
		sin6->sin6_scope_id = 0;
	}
#endif /* __KAME__ */
	freeaddrinfo(res);
}

void
in6_getprefix(const char *plen, int which)
{
	struct sockaddr_in6 *sin6 = sin6tab[which];
	const char *errmsg = NULL;
	u_char *cp;
	int len;

	len = strtonum(plen, 0, 128, &errmsg);
	if (errmsg)
		errx(1, "prefix %s: %s", plen, errmsg);

	sin6->sin6_len = sizeof(*sin6);
	if (which != MASK)
		sin6->sin6_family = AF_INET6;
	if ((len == 0) || (len == 128)) {
		memset(&sin6->sin6_addr, 0xff, sizeof(struct in6_addr));
		return;
	}
	memset((void *)&sin6->sin6_addr, 0x00, sizeof(sin6->sin6_addr));
	for (cp = (u_char *)&sin6->sin6_addr; len > 7; len -= 8)
		*cp++ = 0xff;
	if (len)
		*cp = 0xff << (8 - len);
}

int
prefix(void *val, int size)
{
	u_char *name = (u_char *)val;
	int byte, bit, plen = 0;

	for (byte = 0; byte < size; byte++, plen += 8)
		if (name[byte] != 0xff)
			break;
	if (byte == size)
		return (plen);
	for (bit = 7; bit != 0; bit--, plen++)
		if (!(name[byte] & (1 << bit)))
			break;
	for (; bit != 0; bit--)
		if (name[byte] & (1 << bit))
			return (0);
	byte++;
	for (; byte < size; byte++)
		if (name[byte])
			return (0);
	return (plen);
}
#endif /*INET6*/

/* Print usage, exit(value) if value is non-zero. */
void
usage(int value)
{
	fprintf(stderr,
	    "usage: ifconfig [-AaC] [interface] [address_family] "
	    "[address [dest_address]]\n"
	    "\t\t[parameters]\n");
	exit(value);
}

void
getifgroups(void)
{
	int			 len, cnt;
	struct ifgroupreq	 ifgr;
	struct ifg_req		*ifg;

	memset(&ifgr, 0, sizeof(ifgr));
	strlcpy(ifgr.ifgr_name, name, IFNAMSIZ);

	if (ioctl(s, SIOCGIFGROUP, (caddr_t)&ifgr) == -1) {
		if (errno == EINVAL || errno == ENOTTY)
			return;
		else
			err(1, "SIOCGIFGROUP");
	}

	len = ifgr.ifgr_len;
	ifgr.ifgr_groups =
	    (struct ifg_req *)calloc(len / sizeof(struct ifg_req),
	    sizeof(struct ifg_req));
	if (ifgr.ifgr_groups == NULL)
		err(1, "getifgroups");
	if (ioctl(s, SIOCGIFGROUP, (caddr_t)&ifgr) == -1)
		err(1, "SIOCGIFGROUP");

	cnt = 0;
	ifg = ifgr.ifgr_groups;
	for (; ifg && len >= sizeof(struct ifg_req); ifg++) {
		len -= sizeof(struct ifg_req);
		if (strcmp(ifg->ifgrq_group, "all")) {
			if (cnt == 0)
				printf("\tgroups:");
			cnt++;
			printf(" %s", ifg->ifgrq_group);
		}
	}
	if (cnt)
		printf("\n");

	free(ifgr.ifgr_groups);
}

#ifdef INET6
char *
sec2str(time_t total)
{
	static char result[256];
	int days, hours, mins, secs;
	int first = 1;
	char *p = result;
	char *end = &result[sizeof(result)];
	int n;

	if (0) {	/*XXX*/
		days = total / 3600 / 24;
		hours = (total / 3600) % 24;
		mins = (total / 60) % 60;
		secs = total % 60;

		if (days) {
			first = 0;
			n = snprintf(p, end - p, "%dd", days);
			if (n < 0 || n >= end - p)
				return (result);
			p += n;
		}
		if (!first || hours) {
			first = 0;
			n = snprintf(p, end - p, "%dh", hours);
			if (n < 0 || n >= end - p)
				return (result);
			p += n;
		}
		if (!first || mins) {
			first = 0;
			n = snprintf(p, end - p, "%dm", mins);
			if (n < 0 || n >= end - p)
				return (result);
			p += n;
		}
		snprintf(p, end - p, "%ds", secs);
	} else
		snprintf(p, end - p, "%lu", (u_long)total);

	return (result);
}
#endif /* INET6 */

void
setiflladdr(const char *addr, int param)
{
	struct ether_addr *eap, eabuf;

	if (!strcmp(addr, "random")) {
		arc4random_buf(&eabuf, sizeof eabuf);
		/* Non-multicast and claim it is a hardware address */
		eabuf.ether_addr_octet[0] &= 0xfc;
		eap = &eabuf;
	} else {
		eap = ether_aton(addr);
		if (eap == NULL) {
			warnx("malformed link-level address");
			return;
		}
	}
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_addr.sa_len = ETHER_ADDR_LEN;
	ifr.ifr_addr.sa_family = AF_LINK;
	bcopy(eap, ifr.ifr_addr.sa_data, ETHER_ADDR_LEN);
	if (ioctl(s, SIOCSIFLLADDR, (caddr_t)&ifr) < 0)
		warn("SIOCSIFLLADDR");
}

#ifndef SMALL
void
setinstance(const char *id, int param)
{
	const char *errmsg = NULL;
	int rdomainid;

	rdomainid = strtonum(id, 0, 128, &errmsg);
	if (errmsg)
		errx(1, "rdomain %s: %s", id, errmsg);

	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_rdomainid = rdomainid;
	if (ioctl(s, SIOCSIFRTABLEID, (caddr_t)&ifr) < 0)
		warn("SIOCSIFRTABLEID");
}
#endif
