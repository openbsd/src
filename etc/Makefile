#	$OpenBSD: Makefile,v 1.105 2000/01/05 20:16:36 angelos Exp $

TZDIR=		/usr/share/zoneinfo
LOCALTIME=	US/Pacific

NOOBJ=	oobj

.if exists(etc.${MACHINE}/Makefile.inc)
.include "etc.${MACHINE}/Makefile.inc"
.endif

# -rw-r--r--
BINOWN= root
BINGRP= wheel
BIN1=	aliases bootptab changelist ccd.conf csh.cshrc csh.login csh.logout \
	daily dhcpd.conf dhcpd.interfaces dm.conf exports ftpusers \
	ftpchroot gettytab group hosts hosts.lpd ifaliases inetd.conf \
	ipf.rules ksh.kshrc locate.rc man.conf monthly motd mrouted.conf \
	myname ipnat.rules netstart networks newsyslog.conf passwd.conf \
	phones printcap protocols rbootd.conf rc rc.conf rc.local \
	rc.securelevel rc.shutdown remote rpc rtadvd.conf security services \
	shells syslog.conf weekly etc.${MACHINE}/disktab dhclient.conf \
	mailer.conf

# -rw-rw-r--
BIN2=	motd

NAMEDB=	localhost.rev root.cache
PCS=	pcs750.bin
WCS1=	wcs fppwcs poc poc1 poc2 fppoc
WCS2=	fpevent fppwcs fppwcs_dual hdcwcs load_diags start_fpp wcs wcs_dual

# Use NOGZIP on architectures where the gzip'ing would take too much time
# (pmax or slower :-)).  This way you get only tar'ed snap files and you can
# gzip them on a faster machine
.ifndef NOGZIP
GZIP?=		gzip
GZIPFLAGS?=	-9
GZIPEXT?=	.gz
.else
GZIP=		cat
GZIPFLAGS=
GZIPEXT=
.endif

all clean cleandir depend etc install lint:

.ifndef DESTDIR
distribution-etc-root-var distribution distrib-dirs release snapshot:
	@echo setenv DESTDIR before doing that!
	@false
.else
distribution-etc-root-var: distrib-dirs
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 ${BIN1} ${DESTDIR}/etc
	cat etc.${MACHINE}/ttys ttys.pty > ${DESTDIR}/etc/ttys && \
	    chown ${BINOWN} ${DESTDIR}/etc/ttys && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/ttys && \
	    chmod 644 ${DESTDIR}/etc/ttys
	cat sysctl.conf etc.${MACHINE}/sysctl.conf > ${DESTDIR}/etc/sysctl.conf && \
	    chown ${BINOWN} ${DESTDIR}/etc/sysctl.conf && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/sysctl.conf && \
	    chmod 644 ${DESTDIR}/etc/ttys
	cat fbtab.head etc.${MACHINE}/fbtab fbtab.tail > ${DESTDIR}/etc/fbtab && \
	    chown ${BINOWN} ${DESTDIR}/etc/fbtab && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/fbtab && \
	    chmod 644 ${DESTDIR}/etc/fbtab
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 ${BIN2} ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 hosts.equiv ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 crontab ${DESTDIR}/var/cron/tabs/root
	${INSTALL} -c -o root -g wheel -m 600 master.passwd ${DESTDIR}/etc
	pwd_mkdb -p -d ${DESTDIR}/etc /etc/master.passwd
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	     MAKEDEV.local etc.${MACHINE}/MAKEDEV ${DESTDIR}/dev
	(cd root; \
		${INSTALL} -c -o root -g wheel -m 644 dot.cshrc \
		    ${DESTDIR}/root/.cshrc; \
		${INSTALL} -c -o root -g wheel -m 600 dot.klogin \
		    ${DESTDIR}/root/.klogin; \
		${INSTALL} -c -o root -g wheel -m 644 dot.login \
		    ${DESTDIR}/root/.login; \
		${INSTALL} -c -o root -g wheel -m 644 dot.profile \
		    ${DESTDIR}/root/.profile; \
		rm -f ${DESTDIR}/.cshrc ${DESTDIR}/.profile; \
		ln ${DESTDIR}/root/.cshrc ${DESTDIR}/.cshrc; \
		ln ${DESTDIR}/root/.profile ${DESTDIR}/.profile)
	(cd kerberosIV; \
		${INSTALL} -c -o root -g wheel -m 644 README \
		    ${DESTDIR}/etc/kerberosIV; \
		${INSTALL} -c -o root -g wheel -m 644 krb.conf \
		    ${DESTDIR}/etc/kerberosIV; \
		${INSTALL} -c -o root -g wheel -m 644 krb.realms \
		    ${DESTDIR}/etc/kerberosIV)
	(cd amd; \
		${INSTALL} -c -o root -g wheel -m 644 master.sample \
		    ${DESTDIR}/etc/amd)
	(cd mtree; \
		${INSTALL} -c -o root -g wheel -m 600 special \
		    ${DESTDIR}/etc/mtree; \
		${INSTALL} -c -o root -g wheel -m 444 4.4BSD.dist \
		    ${DESTDIR}/etc/mtree; \
		${INSTALL} -c -o root -g wheel -m 444 BSD.local.dist \
		    ${DESTDIR}/etc/mtree; \
		${INSTALL} -c -o root -g wheel -m 444 BSD.x11.dist \
		    ${DESTDIR}/etc/mtree)
	(cd photuris; \
		${INSTALL} -c -o root -g wheel -m 600 secrets.conf \
		    ${DESTDIR}/etc/photuris; \
		${INSTALL} -c -o root -g wheel -m 600 attributes.conf \
		    ${DESTDIR}/etc/photuris; \
		${INSTALL} -c -o root -g wheel -m 600 photuris.conf \
		    ${DESTDIR}/etc/photuris; \
		${INSTALL} -c -o root -g wheel -m 600 photuris.startup \
		    ${DESTDIR}/etc/photuris)
	(cd ppp; \
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
		${INSTALL} -c -o root -g wheel -m 644 ppp.conf.sample \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 644 ppp.linkup.sample \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 644 ppp.linkdown.sample \
		    ${DESTDIR}/etc/ppp; \
		${INSTALL} -c -o root -g wheel -m 644 ppp.secret.sample \
		    ${DESTDIR}/etc/ppp)
	(cd afs; \
		${INSTALL} -c -o root -g wheel -m 644 afsd.conf \
		    ${DESTDIR}/etc/afs; \
		${INSTALL} -c -o root -g wheel -m 644 ThisCell \
		    ${DESTDIR}/etc/afs; \
		${INSTALL} -c -o root -g wheel -m 644 CellServDB \
		    ${DESTDIR}/etc/afs; \
		${INSTALL} -c -o root -g wheel -m 644 SuidCells \
		    ${DESTDIR}/etc/afs; \
		${INSTALL} -c -o root -g wheel -m 644 README \
		    ${DESTDIR}/etc/afs)
	(cd namedb; \
	    ${INSTALL} -c -o root -g ${BINGRP} -m 644 named.boot \
		${DESTDIR}/var/named; \
	    ${INSTALL} -c -o named -g ${BINGRP} -m 644 ${NAMEDB} \
		${DESTDIR}/var/named/namedb)
	/bin/rm -f ${DESTDIR}/etc/localtime
	ln -s ${TZDIR}/${LOCALTIME} ${DESTDIR}/etc/localtime
	/bin/rm -f ${DESTDIR}/etc/rmt
	ln -s /usr/sbin/rmt ${DESTDIR}/etc/rmt
	${INSTALL} -c -o root -g wheel -m 644 minfree \
		${DESTDIR}/var/crash
	${INSTALL} -c -o ${BINOWN} -g operator -m 664 /dev/null \
		${DESTDIR}/etc/dumpdates
	${INSTALL} -c -o ${BINOWN} -g wheel -m 600 /dev/null \
		${DESTDIR}/etc/skeykeys
	${INSTALL} -c -o root -g wheel -m 600 /dev/null \
		${DESTDIR}/var/at/at.deny
	${INSTALL} -c -o root -g wheel -m 600 /dev/null \
		${DESTDIR}/var/cron/log
	${INSTALL} -c -o root -g wheel -m 444 /dev/null \
		${DESTDIR}/var/db/locate.database
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
		${DESTDIR}/var/log/authlog
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
		${DESTDIR}/var/log/daemon
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
		${DESTDIR}/var/log/ftpd
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
		${DESTDIR}/var/log/ipflog
	${INSTALL} -c -o ${BINOWN} -g wheel -m 644 /dev/null \
		${DESTDIR}/var/log/lastlog
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
		${DESTDIR}/var/log/lpd-errs
	${INSTALL} -c -o ${BINOWN} -g wheel -m 600 /dev/null \
		${DESTDIR}/var/log/maillog
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
		${DESTDIR}/var/log/messages
	${INSTALL} -c -o ${BINOWN} -g wheel -m 600 /dev/null \
		${DESTDIR}/var/log/secure
	${INSTALL} -c -o ${BINOWN} -g wheel -m 644 /dev/null \
		${DESTDIR}/var/log/wtmp
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
		${DESTDIR}/var/log/xferlog
	${INSTALL} -c -o daemon -g staff -m 664 /dev/null \
		${DESTDIR}/var/msgs/bounds
	${INSTALL} -c -o ${BINOWN} -g utmp -m 664 /dev/null \
		${DESTDIR}/var/run/utmp
.if (${MACHINE_ARCH} == "mips")
	(cd etc.${MACHINE}; ${INSTALL} -c -o ${BINOWN} -g wheel -m 444 \
	    ld.so.conf ${DESTDIR}/etc)
.endif
	(cd ${DESTDIR}/dev; ./MAKEDEV all)
.if ${MACHINE} == "vax"
	(cd etc.vax; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${PCS} \
	    ${DESTDIR}/)
.endif
	(cd ../usr.sbin/sendmail/cf/cf; ${MAKE} distribution)
	(cd ../usr.sbin/ypserv/ypinit; ${MAKE} distribution)
	(cd ../usr.bin/ssh; ${MAKE} distribution)
	(cd ../usr.sbin/httpd; ${MAKE} -f Makefile.bsd-wrapper distribution)
	(cd ../lib/libssl; ${MAKE} -f Makefile.bsd-wrapper distribution)
	(cd ../gnu/usr.bin/lynx; ${MAKE} -f Makefile.bsd-wrapper distribution)
	(cd ../usr.bin/mail; ${MAKE} distribution)
	/usr/sbin/sendmail -C${DESTDIR}/etc/sendmail.cf -bi -O AliasFile=${DESTDIR}/etc/aliases
	${INSTALL} -c -o root -g wheel -m 600 root/root.mail \
		${DESTDIR}/var/mail/root

distribution: distribution-etc-root-var
	(cd ..; ${MAKE} install)

distrib-dirs:
	${INSTALL} -d -o root -g wheel -m 755 ${DESTDIR}
	-mtree -qdef mtree/4.4BSD.dist -p ${DESTDIR}/ -u
	if [ ! -d ${DESTDIR}/usr/src ]; then \
		${INSTALL} -d -o root -g wsrc -m 775 ${DESTDIR}/usr/src; \
	fi
	cd ${DESTDIR}/; rm -f sys; ln -s usr/src/sys sys

.ifndef RELEASEDIR
release:
	@echo setenv RELEASEDIR before building a release.
	@false
.else
release: distribution snap_pre snap_md
	cd ${.CURDIR}/../distrib/notes; ${MAKE}; ${MAKE} install
	cd ${.CURDIR}/../distrib/sets; csh maketars ${OSrev}
	-cp ${DESTDIR}/snapshot/bsd* ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/*boot* ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/*BOOT* ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/INSTALL.* ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/*.fs ${DESTDIR}/snapshot/*.fs.gz ${RELEASEDIR}
	-cd ${RELEASEDIR}; \
		md5 bsd!(*.gz) *boot* *BOOT* INSTALL.* *.fs *.gz > MD5
	-cd ${RELEASEDIR}; \
		cksum bsd!(*.gz) *boot* *BOOT* INSTALL.* *.fs *.gz > CKSUM
.if defined(MACHINE_HAS_TOOLS)
	mkdir -p ${RELEASEDIR}/tools
	cp ${DESTDIR}/snapshot/tools/* ${RELEASEDIR}/tools
	cd ${RELEASEDIR} && md5 tools/* >>MD5
	cd ${RELEASEDIR} && cksum tools/* >>CKSUM
.endif
	-cd ${RELEASEDIR} && sort -o MD5 MD5
	-cd ${RELEASEDIR} && sort -o CKSUM -k 3 CKSUM
.endif

snapshot: distribution snap_pre snap_tar snap_md
	cd ${DESTDIR}/snapshot && cksum * > CKSUMS
	cd ${DESTDIR}/snapshot && md5 * > MD5

snap_pre:
	/bin/rm -rf ${DESTDIR}/snapshot
	${INSTALL} -d -o root -g wheel -m 755 ${DESTDIR}/snapshot

snap_tar:
	cd ${DESTDIR} && tar cf - bin \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/bin.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - dev \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/dev.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - .profile .cshrc altroot etc home mnt \
	    root stand sys tmp | ${GZIP} ${GZIPFLAGS} \
	    > snapshot/etc.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - sbin \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/sbin.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - usr/bin \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.bin.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - usr/games \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.games.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - usr/include \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.include.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - usr/lib \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.lib.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - usr/libexec \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.libexec.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - usr/mdec usr/libdata usr/lkm usr/local \
	    usr/src usr/obj | ${GZIP} ${GZIPFLAGS} \
	    > snapshot/usr.misc.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - usr/sbin \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.sbin.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - usr/share \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.share.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - var \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/var.tar${GZIPEXT}
	cd ../distrib/notes; ${MAKE}; ${MAKE} install

snap_md:
# nothing here -- look in the machine-dependent Makefile.inc

.endif	# DESTDIR check

.include <bsd.prog.mk>
