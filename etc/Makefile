#	$OpenBSD: Makefile,v 1.290 2010/06/29 17:17:53 nicm Exp $

TZDIR=		/usr/share/zoneinfo
LOCALTIME=	Canada/Mountain
MTREEDIR=	/etc/mtree

NOOBJ=

.if exists(etc.${MACHINE}/Makefile.inc)
.include "etc.${MACHINE}/Makefile.inc"
.endif

# -rw-r--r--
BINOWN= root
BINGRP= wheel
BIN1=	changelist ccd.conf csh.cshrc csh.login csh.logout daily dhcpd.conf \
	exports ftpusers ftpchroot gettytab group hosts hosts.lpd inetd.conf \
	ksh.kshrc locate.rc man.conf monthly motd mrouted.conf myname \
	netstart networks newsyslog.conf printcap protocols \
	rbootd.conf rc rc.conf rc.local rc.securelevel rc.shutdown \
	remote rpc security services shells syslog.conf weekly \
	etc.${MACHINE}/disktab dhclient.conf mailer.conf ntpd.conf \
	moduli pf.os sensorsd.conf ifstated.conf

.if ${MACHINE} != "aviion" && ${MACHINE} != "mvme68k" && \
    ${MACHINE} != "mvme88k"
BIN1+=	wsconsctl.conf
.endif

# -rw-rw-r--
BIN2=	motd

MISETS=	base${OSrev}.tgz comp${OSrev}.tgz misc${OSrev}.tgz \
	man${OSrev}.tgz game${OSrev}.tgz etc${OSrev}.tgz

PCS=	pcs750.bin

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
	${INSTALL} -c -o root -g wheel -m 600 ${.CURDIR}/mtree/special \
	    ${DESTDIR}${MTREEDIR}
	${INSTALL} -c -o root -g wheel -m 444 ${.CURDIR}/mtree/4.4BSD.dist \
	    ${DESTDIR}${MTREEDIR}
	${INSTALL} -c -o root -g wheel -m 444 ${.CURDIR}/mtree/BSD.local.dist \
	    ${DESTDIR}${MTREEDIR}
	${INSTALL} -c -o root -g wheel -m 444 ${.CURDIR}/mtree/BSD.x11.dist \
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
	awk -f ${.CURDIR}/mklogin.conf `test -f etc.${MACHINE}/login.conf.overrides && echo etc.${MACHINE}/login.conf.overrides` < ${.CURDIR}/login.conf.in > \
	    ${DESTDIR}/etc/login.conf && \
	    chown ${BINOWN}:${BINGRP} ${DESTDIR}/etc/login.conf && \
	    chmod 644 ${DESTDIR}/etc/login.conf
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 ${BIN2} ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 hosts.equiv ${DESTDIR}/etc
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
		${INSTALL} -c -o root -g wheel -m 600 /dev/null \
		    ${DESTDIR}/etc/skel/.ssh/authorized_keys
	cd kerberosV; \
		${INSTALL} -c -o root -g wheel -m 644 README \
		    ${DESTDIR}/etc/kerberosV; \
		${INSTALL} -c -o root -g wheel -m 644 krb5.conf.example \
		    ${DESTDIR}/etc/kerberosV
	cd amd; \
		${INSTALL} -c -o root -g wheel -m 644 master.sample \
		    ${DESTDIR}/etc/amd
	cd ppp; \
		${INSTALL} -c -o root -g wheel -m 600 chap-secrets \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 600 options \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 600 options.leaf \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 600 options.sample \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 600 chatscript.sample \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 600 pap-secrets \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 600 ppp.conf.sample \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 644 ppp.linkup.sample \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 644 ppp.linkdown.sample \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 644 ppp.secret.sample \
		    ${DESTDIR}/etc/ppp
	cd afs; \
		${INSTALL} -c -o root -g wheel -m 644 afsd.conf \
		    ${DESTDIR}/etc/afs; \
		${INSTALL} -c -o root -g wheel -m 644 ThisCell \
		    ${DESTDIR}/etc/afs; \
		${INSTALL} -c -o root -g wheel -m 644 CellServDB \
		    ${DESTDIR}/etc/afs; \
		${INSTALL} -c -o root -g wheel -m 644 SuidCells \
		    ${DESTDIR}/etc/afs; \
		${INSTALL} -c -o root -g wheel -m 644 README \
		    ${DESTDIR}/etc/afs
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
	${INSTALL} -c -o daemon -g staff -m 664 /dev/null \
	    ${DESTDIR}/var/msgs/bounds
	${INSTALL} -c -o ${BINOWN} -g utmp -m 664 /dev/null \
	    ${DESTDIR}/var/run/utmp
.if ${MACHINE} == "vax"
	uudecode -p etc.vax/${PCS}.uu > ${DESTDIR}/${PCS} && \
	    chown ${BINOWN} ${DESTDIR}/${PCS} && \
	    chgrp ${BINGRP} ${DESTDIR}/${PCS} && \
	    chmod 644 ${DESTDIR}/${PCS}
.endif
	cd ../gnu/usr.sbin/sendmail/cf/cf && exec ${MAKE} distribution
	cd ../usr.sbin/ypserv/ypinit && exec ${MAKE} distribution
	cd ../usr.bin/ssh && exec ${MAKE} distribution
	cd ../usr.sbin/httpd && exec ${MAKE} -f Makefile.bsd-wrapper distribution
	cd ../lib/libssl && exec ${MAKE} distribution
	cd ../gnu/usr.bin/lynx && exec ${MAKE} -f Makefile.bsd-wrapper distribution
	cd ../usr.bin/bgplg && exec ${MAKE} distribution
	cd ../usr.bin/mail && exec ${MAKE} distribution
	cd ../usr.sbin/ldapd && exec ${MAKE} distribution
	cd mail && exec ${MAKE} distribution
	${INSTALL} -c -o root -g wheel -m 600 root/root.mail \
	    ${DESTDIR}/var/mail/root
	${INSTALL} -c -o root -g wheel -m 440 ../usr.bin/sudo/sudoers \
	    ${DESTDIR}/etc/sudoers

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
	cd ${.CURDIR}/../distrib/sets && exec ${SUDO} sh maketars ${OSrev}

sha:
	-cd ${RELEASEDIR}; \
	    sum -a sha256 INSTALL.`arch -ks` ${MDEXT} ${MISETS} > SHA256

release: distribution kernels release-sets distrib sha

.endif

.endif	# DESTDIR check

distrib:
	cd ${.CURDIR}/../distrib && \
	    ${MAKE} && exec ${SUDO} ${MAKE} install

DHSIZE=1024 1536 2048 3072 4096
update-moduli:
	( \
		echo -n '#    $$Open'; echo 'BSD$$'; \
		echo '# Time Type Tests Tries Size Generator Modulus'; \
		( for i in ${DHSIZE}; do \
			ssh-keygen -b $$i -G /dev/stdout; \
		done) | \
		ssh-keygen -T /dev/stdout \
	) > moduli

.PHONY: distribution-etc-root-var distribution distrib-dirs \
	release allarchs kernels release-sets m4 install-mtree

SUBDIR+= etc.alpha etc.amd64 etc.armish etc.aviion etc.hp300 etc.hppa
SUBDIR+= etc.hppa64 etc.i386 etc.landisk etc.loongson etc.luna88k 
SUBDIR+= etc.mac68k etc.macppc etc.mvme68k etc.mvme88k etc.palm 
SUBDIR+= etc.sgi etc.socppc etc.sparc etc.sparc64 etc.vax etc.zaurus

.include <bsd.subdir.mk>
.include <bsd.prog.mk>
