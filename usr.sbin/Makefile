#	$OpenBSD: Makefile,v 1.72 2000/10/16 22:57:29 chris Exp $

# not yet done: catman

.include <bsd.own.mk>

SUBDIR=	ac accton adduser amd arp bootpd bootpgw bootpef bootptest \
	chroot config cron dev_mkdb dhcp \
	edquota gspa httpd inetd iostat \
	ipftest ipmon ipsend kgmon kvm_mkdb lpr \
	mailwrapper map-mbone mrinfo mopd moused mrouted mtrace mtree named \
	netgroup_mkdb openssl pkg_install portmap ppp pppd pppoe pstat \
	pwd_mkdb quot quotaon rarpd rbootd rdconfig rdate repquota rmt \
	rpc.bootparamd rpc.lockd rpc.pcnfsd rwhod \
	sa sesd sliplogin slstats spppcontrol spray \
	syslogd tcpdump timed traceroute trpt trsp \
	usbdevs user vipw vnconfig zdump zic

# IPv6
SUBDIR+=faithd ifmcstat ndp rip6query route6d rtadvd rtsold traceroute6

# Arch dependent tools, with manpages
SUBDIR+=iteconfig grfconfig videomode
SUBDIR+=bad144 apm apmd fdformat memconfig
SUBDIR+=eeprom screenblank grfinfo hilinfo
SUBDIR+=wsconscfg wsfontload

.if (${AFS:L} == "yes")
SUBDIR+=afs
.endif

.if (${YP:L} == "yes")
SUBDIR+=ypbind yppoll ypset ypserv
.endif

.include <bsd.subdir.mk>
