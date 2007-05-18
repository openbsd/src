#	$OpenBSD: Makefile,v 1.134 2007/05/18 16:08:12 deraadt Exp $

.include <bsd.own.mk>

SUBDIR=	ac accton acpidump adduser amd apm apmd arp \
	authpf bgpctl bgpd bind chroot config cron dev_mkdb \
	dhcpd dhcrelay dvmrpctl dvmrpd edquota eeprom faithd fdformat \
	ftp-proxy gpioctl hostapd hoststatectl hoststated hotplugd \
	httpd ifstated inetd iostat kgmon kvm_mkdb lpr mailwrapper \
	map-mbone memconfig mopd mrinfo mrouted mtrace mtree ndp \
	netgroup_mkdb ntpd openssl ospfctl ospfd pkg_add popa3d portmap \
	ppp pppd pppoe procmap pstat pwd_mkdb quot quotaon \
	rarpd rbootd rdate rdconfig repquota rip6query ripctl ripd \
	rmt route6d rpc.bootparamd rpc.lockd rtadvd rtsold rwhod sa \
	sasyncd sensorsd sliplogin slstats spamdb spray syslogc syslogd \
	tcpdrop tcpdump timed tokenadm tokeninit traceroute traceroute6 \
	trpt usbdevs user vipw vnconfig watchdogd wsconscfg wsfontload \
	wsmoused zdump zic ztsscale

.if (${AFS:L} == "yes")
SUBDIR+=afs
.endif

.if (${YP:L} == "yes")
SUBDIR+=ypbind yppoll ypset ypserv
.endif

.include <bsd.subdir.mk>

