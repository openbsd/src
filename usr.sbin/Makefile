#	$OpenBSD: Makefile,v 1.124 2005/10/28 20:00:33 deraadt Exp $

.include <bsd.own.mk>

SUBDIR=	ac accton acpid acpidump adduser amd apm apmd arp authpf bad144 \
	bgpctl bgpd bind chroot config cron dev_mkdb dhcpd dhcrelay \
	edquota eeprom fdformat gpioctl hostapd hotplugd httpd ifstated \
	inetd iostat kgmon kvm_mkdb lpr mailwrapper map-mbone memconfig \
	mopd mrinfo mrouted mtrace mtree netgroup_mkdb ntpd openssl ospfctl \
	ospfd pkg_add popa3d portmap ppp pppd pppoe procmap pstat \
	pwd_mkdb quot quotaon rarpd rbootd rdate rdconfig repquota rmt \
	rpc.bootparamd rpc.lockd rwhod sa sasyncd sensorsd sliplogin \
	slstats spamdb spppcontrol spray syslogc syslogd tcpdrop tcpdump \
	timed tokenadm tokeninit traceroute trpt usbdevs user vipw vnconfig \
	watchdogd wsconscfg wsfontload wsmoused zdump zic ztsscale

# IPv6
SUBDIR+=faithd ndp rip6query route6d rtadvd rtsold traceroute6

.if (${AFS:L} == "yes")
SUBDIR+=afs
.endif

.if (${YP:L} == "yes")
SUBDIR+=ypbind yppoll ypset ypserv
.endif

.include <bsd.subdir.mk>

