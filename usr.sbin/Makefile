#	$OpenBSD: Makefile,v 1.111 2004/05/30 20:43:25 deraadt Exp $

.include <bsd.own.mk>

SUBDIR=	ac accton adduser amd arp authpf bgpctl bgpd bind chroot \
	config cron dev_mkdb dhcpd dhcrelay edquota hotplugd httpd \
	inetd iostat kgmon kvm_mkdb lpr mailwrapper mopd mtree mrouted \
	mrinfo mtrace map-mbone netgroup_mkdb openssl pkg_add popa3d \
	portmap ppp pppd pppoe procmap pstat pwd_mkdb quot quotaon \
	rarpd rbootd rdconfig rdate repquota rmt rpc.bootparamd \
	rpc.lockd rwhod sa sensorsd sesd sliplogin slstats spamdb \
	spppcontrol spray syslogc syslogd tcpdrop tcpdump timed \
	tokenadm tokeninit traceroute trpt usbdevs user vipw vnconfig \
	wsmoused zdump zic

# IPv6
SUBDIR+=faithd ndp rip6query route6d rtadvd rtsold traceroute6

# Arch dependent tools, with manpages
SUBDIR+=bad144 apm apmd fdformat memconfig
SUBDIR+=eeprom grfinfo hilinfo
SUBDIR+=wsconscfg wsfontload

# Extended Attribute tools
SUBDIR+=extattrctl setextattr getextattr

.if (${AFS:L} == "yes")
SUBDIR+=afs
.endif

.if (${YP:L} == "yes")
SUBDIR+=ypbind yppoll ypset ypserv
.endif

.include <bsd.subdir.mk>
