#	from: @(#)Makefile	5.6.1.2 (Berkeley) 5/8/91
#	$Id: Makefile,v 1.3 1995/10/23 07:46:00 deraadt Exp $

# not yet done: catman

SUBDIR=	ac accton arp bootpd bootpgw bootpef bootptest \
	chown chroot config cron dev_mkdb \
	diskpart edquota gettable gspa htable inetd iostat kgmon \
	kvm_mkdb lpr map-mbone mrinfo mrouted mtrace mtree named \
	netgroup_mkdb portmap pppd pstat pwd_mkdb quot quotaon \
	rarpd rbootd rdconfig rdate repquota rmt \
	rpc.bootparamd rpc.pcnfsd rwhod \
	sa sendmail sliplogin slstats spray sysctl \
	syslogd tcpdump timed traceroute trpt trsp update \
	vipw vnconfig ypbind yppoll ypset \
	yppush ypserv yptest ypxfr stdhosts makedbm ypinit \
	zdump zic

# should be listed above, but doesn't work on the Alpha.
.if   (${MACHINE_ARCH} != "alpha") || make(clean) || make(cleandir)
SUBDIR+=amd
.endif

.if   make(clean) || make(cleandir)
SUBDIR+=bad144 dbsym eeprom grfinfo hilinfo iteconfig screenblank
.elif ${MACHINE} == "amiga" || ${MACHINE} == "atari"
SUBDIR+=grfconfig iteconfig videomode
.elif ${MACHINE} == "hp300"
SUBDIR+=config.old grfinfo hilinfo
.elif ${MACHINE} == "i386"
SUBDIR+=bad144 
.elif ${MACHINE} == "pmax"
SUBDIR+=config.old
.elif ${MACHINE} == "sun3"
SUBDIR+=dbsym eeprom screenblank
.elif ${MACHINE} == "sparc"
SUBDIR+=eeprom screenblank
.endif

.include <bsd.subdir.mk>
