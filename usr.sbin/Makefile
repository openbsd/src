#	$OpenBSD: Makefile,v 1.80 2001/06/02 18:21:24 deraadt Exp $

# not yet done: catman

.include <bsd.own.mk>

SUBDIR=	ac accton adduser amd arp bootpd bootpgw bootpef bootptest \
	chroot config cron dev_mkdb dhcp \
	edquota gspa httpd inetd iostat \
	kgmon kvm_mkdb lpr \
	mailwrapper mopd mtree named \
	netgroup_mkdb openssl pkg pkg_install portmap ppp pppd pppoe pstat \
	pwd_mkdb quot quotaon rarpd rbootd rdconfig rdate repquota rmt \
	rpc.bootparamd rpc.lockd rwhod \
	sa sesd sliplogin slstats spppcontrol spray \
	syslogd tcpdump timed tokenadm tokeninit traceroute trpt trsp \
	usbdevs user vipw vnconfig wsmoused zdump zic

# LICENSING ISSUES:
# rpc.pcnfsd mrouted mrinfo mtrace map-mbone

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
