#	$OpenBSD: Makefile,v 1.13 1996/06/11 03:25:16 tholo Exp $

# not yet done: catman

SUBDIR=	ac accton arp bootpd bootpgw bootpef bootptest \
	chown chroot config cron dev_mkdb \
	diskpart edquota gettable gspa htable inetd iostat \
	ipftest ipmon ipsend kgmon \
	kvm_mkdb lpr map-mbone mrinfo mrouted mtrace mtree named \
	netgroup_mkdb pkg_install portmap pppd pstat pwd_mkdb quot quotaon \
	rarpd rbootd rdconfig rdate repquota rmt \
	rpc.bootparamd rpc.pcnfsd rwhod \
	sa sendmail sliplogin slstats spray sysctl \
	syslogd tcpdump timed traceroute trpt trsp \
	vipw vnconfig ypbind yppoll ypset \
	ypserv zdump zic

# should be listed above, but doesn't work on the Alpha.
.if   (${MACHINE_ARCH} != "alpha") || make(clean) || make(cleandir)
SUBDIR+=amd
.endif

.if   make(clean) || make(cleandir) || make(obj)
SUBDIR+=apm apmd bad144 config.old dbsym eeprom grfconfig grfinfo \
	hilinfo iteconfig pcmciad screenblank
.elif ${MACHINE} == "amiga" 
SUBDIR+=grfconfig iteconfig screenblank videomode
.elif ${MACHINE} == "atari"
SUBDIR+=iteconfig
.elif ${MACHINE} == "hp300"
SUBDIR+=grfinfo hilinfo config.old
.elif ${MACHINE} == "i386"
SUBDIR+=bad144 apm apmd pcmciad
.elif ${MACHINE} == "pmax"
SUBDIR+=
.elif ${MACHINE} == "sun3"
SUBDIR+=dbsym eeprom screenblank
.elif ${MACHINE} == "sparc"
SUBDIR+=eeprom screenblank
.endif

.include <bsd.subdir.mk>
