#	$OpenBSD: Makefile,v 1.86 2001/12/01 18:57:33 deraadt Exp $

# not yet done: catman

.include <bsd.own.mk>

SUBDIR=	ac accton adduser altq amd arp bootpd bootpgw bootpef bootptest \
	chroot config cron dev_mkdb dhcp \
	edquota gspa httpd inetd iostat \
	kgmon kvm_mkdb lpr \
	mailwrapper mopd mtree named \
	netgroup_mkdb openssl pkg pkg_install popa3d portmap ppp pppd pppoe \
	pstat pwd_mkdb quot quotaon rarpd rbootd rdconfig rdate repquota rmt \
	rpc.bootparamd rpc.lockd rpc.pcnfsd rwhod \
	sa sesd sliplogin slstats spppcontrol spray \
	syslogd tcpdump timed tokenadm tokeninit traceroute trpt trsp \
	usbdevs user vipw vnconfig wsmoused zdump zic

# LICENSING ISSUES: mrouted has non-free parts, and the following use them:
# mrouted mrinfo mtrace map-mbone

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
