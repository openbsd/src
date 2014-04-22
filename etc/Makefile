#	$OpenBSD: Makefile,v 1.354 2014/04/22 10:24:29 reyk Exp $

TZDIR=		/usr/share/zoneinfo
LOCALTIME=	Canada/Mountain
MTREEDIR=	/etc/mtree

NOOBJ=

KERNELS = GENERIC bsd

.if exists(etc.${MACHINE}/Makefile.inc)
.include "etc.${MACHINE}/Makefile.inc"
.endif

.for CONF K in ${KERNELS}
.  if !target($K)
$K:
	cd ../sys/arch/${MACHINE}/conf && config ${CONF}
	cd ../sys/arch/${MACHINE}/compile/${CONF} && \
	    ${MAKE} clean && exec ${MAKE}
.  endif
ALL_KERNELS += $K
.endfor

kernels: bootblocks ${ALL_KERNELS}
.for CONF K in ${KERNELS}
	cp ../sys/arch/${MACHINE}/compile/${CONF}/bsd ${RELEASEDIR}/$K
.endfor

# -rw-r--r--
BINOWN= root
BINGRP= wheel
BIN1=	changelist csh.cshrc csh.login csh.logout daily dhcpd.conf \
	exports ftpusers ftpchroot gettytab group hosts hosts.lpd inetd.conf \
	ksh.kshrc locate.rc man.conf monthly motd mrouted.conf myname \
	netstart networks newsyslog.conf printcap protocols \
	rbootd.conf rc rc.conf rc.local rc.securelevel rc.shutdown \
	remote rpc services shells syslog.conf weekly \
	etc.${MACHINE}/login.conf \
	etc.${MACHINE}/disktab dhclient.conf mailer.conf ntpd.conf \
	moduli pf.os sensorsd.conf ifstated.conf mixerctl.conf

.if ${MACHINE} != "aviion" 
BIN1+=	wsconsctl.conf
.endif

# -rw-rw-r--
BIN2=	motd

# -r-xr-xr-x
RCDAEMONS=	amd apmd bgpd bootparamd cron dhcpd dhcrelay dvmrpd \
		ftpd ftpproxy hostapd hotplugd identd ifstated iked \
		inetd isakmpd ldapd npppd ldattach ldpd lpd mopd mrouted \
		named nginx nsd ntpd ospfd ospf6d portmap pflogd rarpd rbootd \
		relayd ripd route6d rtadvd rtsold rwhod sasyncd sendmail \
		sensorsd slowcgi smtpd snmpd spamd sshd syslogd watchdogd \
		wsmoused xdm ypbind ypldap yppasswdd ypserv kdc kadmind \
		kpasswdd nfsd mountd lockd statd spamlogd sndiod \
		tftpd tftpproxy ldomd ipropd_master ipropd_slave unbound

MISETS=	base${OSrev}.tgz comp${OSrev}.tgz \
	man${OSrev}.tgz game${OSrev}.tgz etc${OSrev}.tgz

# Use NOGZIP on architectures where the gzip'ing would take too much time
# (pmax or slower :-)).  This way you get only tar'ed snap files and you can
# gzip them on a faster machine
.ifndef NOGZIP
GZIPCMD?=	gzip
GZIPFLAGS?=	-9
GZIPEXT?=	.gz
.else
GZIPCMD=	cat
GZIPFLAGS=
GZIPEXT=
.endif

all clean cleandir depend etc install lint:

install-mtree:
	${INSTALL} -c -o root -g wheel -m 600 mtree/special \
	    ${DESTDIR}${MTREEDIR}
	${INSTALL} -c -o root -g wheel -m 444 mtree/4.4BSD.dist \
	    ${DESTDIR}${MTREEDIR}
	${INSTALL} -c -o root -g wheel -m 444 mtree/BSD.local.dist \
	    ${DESTDIR}${MTREEDIR}
	${INSTALL} -c -o root -g wheel -m 444 mtree/BSD.x11.dist \
	    ${DESTDIR}${MTREEDIR}

.ifndef DESTDIR
distribution-etc-root-var distribution distrib-dirs release:
	@echo setenv DESTDIR before doing that!
	@false
.else
distribution-etc-root-var: distrib-dirs
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 ${BIN1} ${DESTDIR}/etc
	ksh ttys.pty | cat etc.${MACHINE}/ttys - > ${DESTDIR}/etc/ttys && \
	    chown ${BINOWN} ${DESTDIR}/etc/ttys && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/ttys && \
	    chmod 644 ${DESTDIR}/etc/ttys
	cat sysctl.conf etc.${MACHINE}/sysctl.conf > ${DESTDIR}/etc/sysctl.conf && \
	    chown ${BINOWN} ${DESTDIR}/etc/sysctl.conf && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/sysctl.conf && \
	    chmod 644 ${DESTDIR}/etc/sysctl.conf
	cat fbtab.head etc.${MACHINE}/fbtab fbtab.tail > ${DESTDIR}/etc/fbtab && \
	    chown ${BINOWN} ${DESTDIR}/etc/fbtab && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/fbtab && \
	    chmod 644 ${DESTDIR}/etc/fbtab
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 ${BIN2} ${DESTDIR}/etc
	${INSTALL} -c -o root -g crontab -m 600 crontab ${DESTDIR}/var/cron/tabs/root
	${INSTALL} -c -o root -g wheel -m 600 master.passwd ${DESTDIR}/etc
	pwd_mkdb -p -d ${DESTDIR}/etc /etc/master.passwd
	${INSTALL} -c -o root -g wheel -m 600 bgpd.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 ospfd.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 ospf6d.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 ripd.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 dvmrpd.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 ldpd.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 pf.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g operator -m 644 chio.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 hostapd.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 relayd.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 iked.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 ipsec.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 sasyncd.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 snmpd.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 ldapd.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 ypldap.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g _nsd -m 640 nsd.conf ${DESTDIR}/var/nsd/etc
	${INSTALL} -c -o root -g wheel -m 644 unbound.conf ${DESTDIR}/var/unbound/etc
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	    etc.${MACHINE}/MAKEDEV ${DESTDIR}/dev
	cd root; \
		${INSTALL} -c -o root -g wheel -m 644 dot.cshrc \
		    ${DESTDIR}/root/.cshrc; \
		${INSTALL} -c -o root -g wheel -m 600 dot.klogin \
		    ${DESTDIR}/root/.klogin; \
		${INSTALL} -c -o root -g wheel -m 644 dot.login \
		    ${DESTDIR}/root/.login; \
		${INSTALL} -c -o root -g wheel -m 644 dot.profile \
		    ${DESTDIR}/root/.profile; \
		${INSTALL} -c -o root -g wheel -m 644 dot.Xdefaults \
		    ${DESTDIR}/root/.Xdefaults; \
		${INSTALL} -c -o root -g wheel -m 644 dot.cvsrc \
		    ${DESTDIR}/root/.cvsrc; \
		rm -f ${DESTDIR}/.cshrc ${DESTDIR}/.profile; \
		${INSTALL} -c -o root -g wheel -m 644 dot.cshrc \
		    ${DESTDIR}/.cshrc; \
		${INSTALL} -c -o root -g wheel -m 644 dot.profile \
		    ${DESTDIR}/.profile
	cd skel; \
		${INSTALL} -c -o root -g wheel -m 644 dot.cshrc \
		    ${DESTDIR}/etc/skel/.cshrc; \
		${INSTALL} -c -o root -g wheel -m 644 dot.login \
		    ${DESTDIR}/etc/skel/.login; \
		${INSTALL} -c -o root -g wheel -m 644 dot.mailrc \
		    ${DESTDIR}/etc/skel/.mailrc; \
		${INSTALL} -c -o root -g wheel -m 644 dot.profile \
		    ${DESTDIR}/etc/skel/.profile; \
		${INSTALL} -c -o root -g wheel -m 644 dot.Xdefaults \
		    ${DESTDIR}/etc/skel/.Xdefaults; \
		${INSTALL} -c -o root -g wheel -m 644 dot.cvsrc \
		    ${DESTDIR}/etc/skel/.cvsrc; \
		${INSTALL} -c -o root -g wheel -m 600 /dev/null \
		    ${DESTDIR}/etc/skel/.ssh/authorized_keys
	cd amd; \
		${INSTALL} -c -o root -g wheel -m 644 master.sample \
		    ${DESTDIR}/etc/amd
	cd ppp; \
		${INSTALL} -c -o root -g wheel -m 600 chap-secrets \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 600 options \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 600 options.sample \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 600 chatscript.sample \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 600 pap-secrets \
		    ${DESTDIR}/etc/ppp
	cd signify; \
		${INSTALL} -c -o root -g wheel -m 644 *.pub \
		    ${DESTDIR}/etc/signify
	cd systrace; \
		${INSTALL} -c -o root -g wheel -m 600 usr_sbin_lpd \
		    ${DESTDIR}/etc/systrace; \
		${INSTALL} -c -o root -g wheel -m 600 usr_sbin_named \
		    ${DESTDIR}/etc/systrace
	cd bind; \
		${INSTALL} -c -o root -g named -m 640 named-simple.conf \
		    ${DESTDIR}/var/named/etc/named.conf; \
		${INSTALL} -c -o root -g named -m 640 named-*.conf \
		    ${DESTDIR}/var/named/etc; \
		${INSTALL} -c -o root -g wheel -m 644 root.hint \
		    ${DESTDIR}/var/named/etc; \
		${INSTALL} -c -o root -g wheel -m 644 db.localhost \
		    ${DESTDIR}/var/named/standard/localhost; \
		${INSTALL} -c -o root -g wheel -m 644 db.loopback \
		    ${DESTDIR}/var/named/standard/loopback; \
		${INSTALL} -c -o root -g wheel -m 644 db.loopback6.arpa \
		    ${DESTDIR}/var/named/standard/loopback6.arpa
	/bin/rm -f ${DESTDIR}/etc/localtime
	ln -s ${TZDIR}/${LOCALTIME} ${DESTDIR}/etc/localtime
	/bin/rm -f ${DESTDIR}/etc/rmt
	ln -s /usr/sbin/rmt ${DESTDIR}/etc/rmt
	${INSTALL} -c -o root -g wheel -m 644 minfree \
	    ${DESTDIR}/var/crash
	${INSTALL} -c -o ${BINOWN} -g operator -m 664 /dev/null \
	    ${DESTDIR}/etc/dumpdates
	${INSTALL} -c -o root -g crontab -m 660 /dev/null \
	    ${DESTDIR}/var/cron/at.deny
	${INSTALL} -c -o root -g crontab -m 660 /dev/null \
	    ${DESTDIR}/var/cron/cron.deny
	${INSTALL} -c -o root -g wheel -m 600 /dev/null \
	    ${DESTDIR}/var/cron/log
	${INSTALL} -c -o root -g wheel -m 444 /dev/null \
	    ${DESTDIR}/var/db/locate.database
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
	    ${DESTDIR}/var/log/authlog
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
	    ${DESTDIR}/var/log/daemon
	${INSTALL} -c -o ${BINOWN} -g wheel -m 600 /dev/null \
	    ${DESTDIR}/var/log/failedlogin
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
	    ${DESTDIR}/var/log/ftpd
	${INSTALL} -c -o ${BINOWN} -g wheel -m 644 /dev/null \
	    ${DESTDIR}/var/log/lastlog
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
	    ${DESTDIR}/var/log/lpd-errs
	${INSTALL} -c -o ${BINOWN} -g wheel -m 600 /dev/null \
	    ${DESTDIR}/var/log/maillog
	${INSTALL} -c -o ${BINOWN} -g wheel -m 644 /dev/null \
	    ${DESTDIR}/var/log/messages
	${INSTALL} -c -o ${BINOWN} -g wheel -m 600 /dev/null \
	    ${DESTDIR}/var/log/secure
	${INSTALL} -c -o ${BINOWN} -g wheel -m 664 /dev/null \
	    ${DESTDIR}/var/log/sendmail.st
	${INSTALL} -c -o ${BINOWN} -g wheel -m 644 /dev/null \
	    ${DESTDIR}/var/log/wtmp
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
	    ${DESTDIR}/var/log/xferlog
	${INSTALL} -c -o ${BINOWN} -g utmp -m 664 /dev/null \
	    ${DESTDIR}/var/run/utmp
	cd ../gnu/usr.sbin/sendmail/cf/cf && exec ${MAKE} distribution
	cd ../usr.sbin/ypserv/ypinit && exec ${MAKE} distribution
	cd ../usr.bin/ssh && exec ${MAKE} distribution
	cd ../lib/libcrypto && exec ${MAKE} distribution
	cd ../gnu/usr.bin/lynx && exec ${MAKE} -f Makefile.bsd-wrapper distribution
	cd ../usr.bin/bgplg && exec ${MAKE} distribution
	cd ../usr.bin/mail && exec ${MAKE} distribution
	cd ../usr.sbin/ldapd && exec ${MAKE} distribution
	cd ../usr.sbin/nginx && exec ${MAKE} -f Makefile.bsd-wrapper distribution
	cd ../usr.sbin/npppd && exec ${MAKE} distribution
	cd mail && exec ${MAKE} distribution
	${INSTALL} -c -o root -g wheel -m 600 root/root.mail \
	    ${DESTDIR}/var/mail/root
	${INSTALL} -c -o root -g wheel -m 440 ../usr.bin/sudo/sudoers \
	    ${DESTDIR}/etc/sudoers
	cd rc.d; \
		${INSTALL} -c -o root -g wheel -m 644 rc.subr \
		    ${DESTDIR}/etc/rc.d && \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 ${RCDAEMONS} \
		    ${DESTDIR}/etc/rc.d

distribution:
	exec ${SUDO} ${MAKE} distribution-etc-root-var
	cd .. && exec ${SUDO} ${MAKE} install
	touch ${DESTDIR}/var/db/sysmerge/etcsum
	TMPSUM=`mktemp /tmp/_etcsum.XXXXXXXXXX` || exit 1; \
	sort ../distrib/sets/lists/etc/{mi,md.${MACHINE}} > $${TMPSUM}; \
	cd ${DESTDIR} && \
		xargs cksum < $${TMPSUM} > ${DESTDIR}/var/db/sysmerge/etcsum; \
	rm -f $${TMPSUM}

distrib-dirs:
	if [ ! -d ${DESTDIR}/. ]; then \
		${INSTALL} -d -o root -g wheel -m 755 ${DESTDIR}; \
	fi
	mtree -qdef mtree/4.4BSD.dist -p ${DESTDIR}/ -U
	if [ ! -d ${DESTDIR}/usr/src ]; then \
		${INSTALL} -d -o root -g wsrc -m 775 ${DESTDIR}/usr/src; \
	fi
	cd ${DESTDIR}/; rm -f sys; ln -s usr/src/sys sys

.ifndef RELEASEDIR
release:
	@echo setenv RELEASEDIR before building a release.
	@false
.else

release-sets:
	cd ${RELEASEDIR} && rm -f SHA256
	cd ../distrib/sets && exec ${SUDO} sh maketars ${OSrev}

sha:
	-cd ${RELEASEDIR}; \
	    cksum -a sha256 INSTALL.`arch -ks` \
	    ${ALL_KERNELS} ${MDEXT} ${MISETS} | sort > SHA256

release: sha
sha: distrib
distrib: release-sets kernels
release-sets: distribution kernels

.endif

.endif	# DESTDIR check

distrib:
	cd ../distrib && \
	    ${MAKE} && exec ${SUDO} ${MAKE} install

.PHONY: distribution-etc-root-var distribution distrib-dirs \
	release allarchs kernels release-sets m4 install-mtree \
	bootblocks ${ALL_KERNELS}

SUBDIR+= etc.alpha etc.amd64 etc.armish etc.armv7 etc.aviion
SUBDIR+= etc.hppa etc.hppa64 etc.i386 etc.landisk etc.loongson etc.luna88k
SUBDIR+= etc.macppc etc.octeon
SUBDIR+= etc.sgi etc.socppc etc.sparc etc.sparc64 etc.vax etc.zaurus

.include <bsd.subdir.mk>
.include <bsd.prog.mk>
