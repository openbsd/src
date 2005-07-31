#	$OpenBSD: Makefile,v 1.122 2005/07/31 06:25:44 dlg Exp $

.include <bsd.own.mk>

SUBDIR=	ac accton adduser amd arp authpf bgpctl bgpd bind chroot \
	config cron dev_mkdb dhcpd dhcrelay edquota hostapd hotplugd \
	httpd ifstated inetd iostat kgmon kvm_mkdb lpr mailwrapper mopd mtree \
	mrouted mrinfo mtrace map-mbone netgroup_mkdb ntpd openssl \
	ospfctl ospfd pkg_add popa3d portmap ppp pppd pppoe procmap \
	pstat pwd_mkdb quot quotaon rarpd rbootd rdconfig rdate repquota \
	rmt rpc.bootparamd rpc.lockd rwhod sa sasyncd sensorsd sliplogin \
	slstats spamdb spppcontrol spray syslogc syslogd tcpdrop \
	tcpdump timed tokenadm tokeninit traceroute trpt usbdevs user \
	vipw vnconfig wsmoused zdump zic ztsscale

# IPv6
SUBDIR+=faithd ndp rip6query route6d rtadvd rtsold traceroute6

# Arch dependent tools, with manpages
SUBDIR+=bad144 acpid acpidump apm apmd fdformat memconfig
SUBDIR+=eeprom gpioctl hotplugd
SUBDIR+=wsconscfg wsfontload

.if (${AFS:L} == "yes")
SUBDIR+=afs
.endif

.if (${YP:L} == "yes")
SUBDIR+=ypbind yppoll ypset ypserv
.endif

.include <bsd.subdir.mk>
