#	$OpenBSD: Makefile,v 1.92 2002/12/31 16:22:26 miod Exp $

# not yet done: catman

.include <bsd.own.mk>

SUBDIR=	ac accton adduser amd arp authpf \
	bootpd bootpgw bootpef bootptest \
	chroot config cron dev_mkdb dhcp \
	edquota httpd inetd iostat \
	kgmon kvm_mkdb lpr \
	mailwrapper mopd mtree named \
	netgroup_mkdb openssl pkg pkg_install popa3d portmap ppp pppd pppoe \
	pstat pwd_mkdb quot quotaon rarpd rbootd rdconfig rdate repquota rmt \
	rpc.bootparamd rpc.lockd rpc.pcnfsd rwhod \
	sa sesd sliplogin slstats spppcontrol spray \
	syslogd tcpdump timed tokenadm tokeninit traceroute trpt \
	usbdevs user vipw vnconfig wsmoused zdump zic

# LICENSING ISSUES: mrouted has non-free parts, and the following use them:
# mrouted mrinfo mtrace map-mbone

# IPv6
SUBDIR+=faithd ndp rip6query route6d rtadvd rtsold traceroute6

# Arch dependent tools, with manpages
SUBDIR+=bad144 apm apmd fdformat memconfig
SUBDIR+=eeprom grfinfo hilinfo
SUBDIR+=wsconscfg wsfontload

# Extended Attribute tools
SUBDIR+=extattrctl setextattr getextattr

.if (${AFS:L} == "yes")
SUBDIR+=afs
.endif

.if (${YP:L} == "yes")
SUBDIR+=ypbind yppoll ypset ypserv
.endif

.include <bsd.subdir.mk>
