#	$OpenBSD: Makefile,v 1.150 2010/06/03 17:49:47 martinh Exp $

.include <bsd.own.mk>

SUBDIR=	ac accton acpidump adduser amd apm apmd arp \
	authpf bgpctl bgpd bind chroot config cron crunchgen dev_mkdb \
	dhcpd dhcrelay dvmrpctl dvmrpd edquota eeprom faithd fdformat \
	ftp-proxy gpioctl hostapd hotplugd httpd ifstated inetd iostat \
	kgmon kvm_mkdb ldapd ldapctl ldpd ldpctl lpr mailwrapper map-mbone \
	memconfig mksuncd mopd mrinfo mrouted mtrace mtree ndp netgroup_mkdb \
	ntpd openssl ospfctl ospfd ospf6d ospf6ctl pcidump pkg_add popa3d \
	portmap ppp pppd pppoe procmap pstat pwd_mkdb quot quotaon rarpd \
	rbootd rdate rdconfig relayctl relayd repquota rip6query ripctl ripd \
	rmt route6d rpc.bootparamd rpc.lockd rpc.statd rtadvd rtsold rwhod \
	sa sasyncd sensorsd sliplogin slstats smtpd snmpctl snmpd spamdb \
	spray syslogc syslogd sysmerge tcpdrop tcpdump timed tokenadm \
	tokeninit traceroute traceroute6 trpt usbdevs user vipw watchdogd \
	wsconscfg wsfontload wsmoused ypldap zdump zic ztsscale

.if (${AFS:L} == "yes")
SUBDIR+=afs
.endif

.if (${YP:L} == "yes")
SUBDIR+=ypbind yppoll ypset ypserv
.endif

.include <bsd.subdir.mk>

