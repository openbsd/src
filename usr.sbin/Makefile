#	$OpenBSD: Makefile,v 1.21 1996/09/28 05:58:34 downsj Exp $

# not yet done: catman

.include <bsd.own.mk>

SUBDIR=	ac accton adduser arp bootpd bootpgw bootpef bootptest \
	chown chroot config cron dev_mkdb \
	diskpart edquota gspa inetd iostat \
	ipftest ipmon ipsend kgmon \
	kvm_mkdb lpr map-mbone mrinfo mopd mrouted mtrace mtree named \
	netgroup_mkdb pkg_install portmap pppd pstat pwd_mkdb quot quotaon \
	rarpd rbootd rdconfig rdate repquota rmt \
	rpc.bootparamd rpc.pcnfsd rwhod \
	sa sendmail sliplogin slstats spray sysctl \
	syslogd tcpdump timed traceroute trpt trsp \
	vipw vnconfig zdump zic

.if defined (YP)
SUBDIR+=ypbind yppoll ypset ypserv
.endif

# should be listed above, but doesn't work on the Alpha.
.if   (${MACHINE_ARCH} != "alpha") || make(clean) || make(cleandir)
SUBDIR+=amd
.endif

.if   make(clean) || make(cleandir) || make(obj)
SUBDIR+=apm apmd bad144 config.old dbsym eeprom fdformat grfconfig grfinfo \
	hilinfo iteconfig pcmciad screenblank videomode
.elif ${MACHINE} == "amiga" 
SUBDIR+=grfconfig iteconfig screenblank videomode
.elif ${MACHINE} == "atari"
SUBDIR+=iteconfig
.elif ${MACHINE} == "hp300"
SUBDIR+=grfinfo hilinfo config.old
.elif ${MACHINE} == "i386"
SUBDIR+=bad144 apm apmd fdformat pcmciad
.elif ${MACHINE} == "pmax"
SUBDIR+=
.elif ${MACHINE} == "sun3"
SUBDIR+=dbsym eeprom screenblank
.elif ${MACHINE} == "sparc"
SUBDIR+=eeprom screenblank
.endif

.include <bsd.subdir.mk>
