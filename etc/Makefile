#	$OpenBSD: Makefile,v 1.47 1997/04/25 18:50:01 deraadt Exp $

TZDIR=		/usr/share/zoneinfo
LOCALTIME=	US/Pacific

NOOBJ=	oobj

.if exists(etc.${MACHINE}/Makefile.inc)
.include "etc.${MACHINE}/Makefile.inc"
.endif

# disktab may be wrong -- hcx9 is a tahoe, but gets its own.
# -rw-r--r--
BINOWN= root
BINGRP= wheel
BIN1=	aliases bootptab changelist ccd.conf csh.cshrc csh.login csh.logout \
	daily dm.conf exports ftpusers ftpchroot gettytab group hosts \
	hosts.equiv hosts.lpd ifaliases inetd.conf ipf.rules ksh.kshrc \
	locate.rc man.conf monthly motd mrouted.conf myname nat.rules \
	netstart networks newsyslog.conf passwd.conf phones printcap \
	protocols rbootd.conf rc rc.local rc.securelevel remote rpc security \
	services shells syslog.conf virc weekly etc.${MACHINE}/disktab

# -rw-rw-r--
BIN2=	motd

NAMEDB=	localhost.rev named.boot root.cache
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
distribution-etc-root-var distribution distrib-dirs snapshot:
	@echo setenv DESTDIR before doing that!
	@false
.else
distribution-etc-root-var: distrib-dirs
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 ${BIN1} ${DESTDIR}/etc
	cat etc.${MACHINE}/ttys ttys.pty > ${DESTDIR}/etc/ttys && \
	    chown ${BINOWN} ${DESTDIR}/etc/ttys && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/ttys && \
	    chmod 644 ${DESTDIR}/etc/ttys
	cat fbtab.head etc.${MACHINE}/fbtab fbtab.tail > ${DESTDIR}/etc/fbtab && \
	    chown ${BINOWN} ${DESTDIR}/etc/fbtab && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/fbtab && \
	    chmod 644 ${DESTDIR}/etc/fbtab
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 ${BIN2} ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 crontab ${DESTDIR}/var/cron/tabs/root
	${INSTALL} -c -o root -g wheel -m 600 master.passwd ${DESTDIR}/etc
	pwd_mkdb -p -d ${DESTDIR}/etc /etc/master.passwd
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	     MAKEDEV.local etc.${MACHINE}/MAKEDEV ${DESTDIR}/dev
	(cd root; \
		${INSTALL} -c -o root -g wheel -m 644 dot.cshrc \
		    ${DESTDIR}/root/.cshrc; \
		${INSTALL} -c -o root -g wheel -m 644 dot.klogin \
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
		${INSTALL} -c -o root -g wheel -m 444 krb.realms \
		    ${DESTDIR}/etc/kerberosIV)
	(cd amd; \
		${INSTALL} -c -o root -g wheel -m 644 master.sample \
		    ${DESTDIR}/etc/amd)
	(cd mtree; \
		${INSTALL} -c -o root -g wheel -m 600 special \
		    ${DESTDIR}/etc/mtree; \
		${INSTALL} -c -o root -g wheel -m 444 4.4BSD.dist \
		    ${DESTDIR}/etc/mtree)
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
		    ${DESTDIR}/etc/ppp)
	cd namedb; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 ${NAMEDB} \
	    ${DESTDIR}/etc/namedb
	/bin/rm -f ${DESTDIR}/etc/localtime
	ln -s ${TZDIR}/${LOCALTIME} ${DESTDIR}/etc/localtime
	/bin/rm -f ${DESTDIR}/etc/rmt
	ln -s /usr/sbin/rmt ${DESTDIR}/etc/rmt
	${INSTALL} -c -o root -g wheel -m 644 minfree \
		${DESTDIR}/var/crash
	${INSTALL} -c -o ${BINOWN} -g operator -m 664 /dev/null \
		${DESTDIR}/etc/dumpdates
	${INSTALL} -c -o ${BINOWN} -g operator -m 644 /dev/null \
		${DESTDIR}/etc/skeykeys
	${INSTALL} -c -o root -g wheel -m 600 /dev/null \
		${DESTDIR}/var/at/at.deny
	${INSTALL} -c -o root -g wheel -m 600 /dev/null \
		${DESTDIR}/var/cron/log
	${INSTALL} -c -o nobody -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/db/locate.database
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/authlog
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/daemon
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/ftpd
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/ipflog
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/lastlog
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/lpd-errs
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/maillog
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/messages
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/secure
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/wtmp
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/xferlog
	${INSTALL} -c -o daemon -g staff -m 664 /dev/null \
		${DESTDIR}/var/msgs/bounds
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/run/utmp
	(cd etc.${MACHINE}; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    fstab.* ${DESTDIR}/etc)
	(cd ${DESTDIR}/dev; ./MAKEDEV all)
.if ${MACHINE} == "vax"
	(cd etc.vax; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${PCS} \
	    ${DESTDIR}/)
.endif
	(cd ../usr.sbin/sendmail/cf/cf; ${MAKE} distribution)
	(cd ../usr.sbin/ypserv/ypinit; ${MAKE} distribution)

distribution: distribution-etc-root-var
	(cd ..; ${MAKE} install)

hcx9-distribution:
	(cd etc.tahoe; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${WCS2} \
	    ${DESTDIR}/)

distrib-dirs:
	${INSTALL} -d -o root -g wheel -m 755 ${DESTDIR}
	-mtree -def mtree/4.4BSD.dist -p ${DESTDIR}/ -u
	if [ ! -d ${DESTDIR}/usr/src ]; then \
		${INSTALL} -d -o root -g wsrc -m 775 ${DESTDIR}/usr/src; \
	fi
	cd ${DESTDIR}; rm -f sys; ln -s usr/src/sys sys

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
	cd ${DESTDIR} && tar cf - usr/mdec usr/libdata usr/local usr/src \
	    usr/obj | ${GZIP} ${GZIPFLAGS} \
	    > snapshot/usr.misc.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - usr/sbin \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.sbin.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - usr/share \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.share.tar${GZIPEXT}
	cd ${DESTDIR} && tar cf - var \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/var.tar${GZIPEXT}

snap_md:
# nothing here -- look in the machine-dependent Makefile.inc

.endif	# DESTDIR check

.include <bsd.prog.mk>
