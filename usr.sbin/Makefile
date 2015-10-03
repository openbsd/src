#	$OpenBSD: Makefile,v 1.187 2015/10/03 18:57:11 renato Exp $

.include <bsd.own.mk>

SUBDIR=	ac accton acpidump adduser amd apm apmd arp \
	authpf bgpctl bgpd bind chroot config cron crunchgen dev_mkdb \
	dhcpd dhcrelay dvmrpctl dvmrpd edquota eeprom eigrpd eigrpctl \
	fdformat ftp-proxy gpioctl hostapd hotplugd httpd identd ifstated \
	ikectl inetd installboot iostat iscsictl iscsid kgmon kvm_mkdb \
	ldapd ldapctl ldomctl ldomd ldpd ldpctl lpr mailwrapper map-mbone \
	memconfig mksuncd mkuboot mopd mrinfo mrouted \
	mtrace mtree ndp netgroup_mkdb \
	npppctl npppd nsd ntpd ospfctl ospfd ospf6d ospf6ctl \
	pcidump pkg_add portmap pppd procmap pstat pwd_mkdb \
	quot quotaon radiusctl radiusd rarpd rbootd rcctl rdate relayctl \
	relayd repquota rip6query ripctl ripd rmt route6d rpc.bootparamd \
	rpc.lockd rpc.statd rtadvd \
	sa sasyncd sensorsd slowcgi smtpd \
	snmpctl snmpd spamdb syslogc syslogd sysmerge tcpdrop tcpdump \
	tftp-proxy tftpd tokenadm tokeninit traceroute trpt \
	unbound usbdevs user vipw watchdogd wsconscfg wsfontload wsmoused \
	zdump zic ztsscale

.if (${YP:L} == "yes")
SUBDIR+=ypbind ypldap yppoll ypset ypserv
.endif

.include <bsd.subdir.mk>
