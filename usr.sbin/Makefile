#	$OpenBSD: Makefile,v 1.57 2000/02/03 21:54:39 jakob Exp $

# not yet done: catman

.include <bsd.own.mk>

SUBDIR=	ac accton adduser amd arp bootpd bootpgw bootpef bootptest \
	chown chroot config cron dev_mkdb dhcp \
	edquota gspa httpd inetd iostat \
	ipftest ipmon ipsend kgmon kvm_mkdb lpr openssl\
	mailwrapper map-mbone mrinfo mopd mrouted mtrace mtree named \
	netgroup_mkdb pkg_install portmap ppp pppd pstat pwd_mkdb \
	quot quotaon rarpd rbootd rdconfig rdate repquota rmt \
	rpc.bootparamd rpc.lockd rpc.pcnfsd rwhod \
	sa sendmail sliplogin slstats spray \
	syslogd tcpdump timed traceroute trpt trsp \
	usbdevs vipw vnconfig zdump zic

# IPv6
SUBDIR+=faithd ifmcstat ndp rip6query route6d rtadvd rtsold

.if (${AFS} == "yes")
SUBDIR+=afs
.endif

.if (${YP} == "yes")
SUBDIR+=ypbind yppoll ypset ypserv
.endif

.if   make(clean) || make(cleandir) || make(obj)
SUBDIR+=apm apmd bad144 eeprom fdformat grfconfig grfinfo \
	hilinfo iteconfig pccon screenblank videomode memconfig
.elif ${MACHINE} == "amiga" 
SUBDIR+=grfconfig iteconfig videomode
.elif ${MACHINE} == "atari"
SUBDIR+=iteconfig
.elif ${MACHINE} == "hp300"
SUBDIR+=grfinfo hilinfo
.elif ${MACHINE} == "i386"
SUBDIR+=bad144 apm apmd fdformat pccon memconfig
.elif ${MACHINE} == "pmax"
SUBDIR+=
.elif ${MACHINE} == "sun3"
SUBDIR+=eeprom screenblank
.elif ${MACHINE} == "sparc"
SUBDIR+=eeprom fdformat screenblank
.endif

.include <bsd.subdir.mk>
