#	$OpenBSD: Makefile,v 1.34 1997/01/03 04:04:52 etheisen Exp $

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
	locate.rc man.conf monthly motd mrouted.conf myname netstart \
	networks newsyslog.conf phones printcap protocols rbootd.conf rc \
	rc.local remote rpc security services shells syslog.conf virc weekly \
	etc.${MACHINE}/disktab

# -rw-rw-r--
BIN2=	motd

NAMEDB=	localhost.rev named.boot root.cache
PCS=	pcs750.bin
WCS1=	wcs fppwcs poc poc1 poc2 fppoc
WCS2=	fpevent fppwcs fppwcs_dual hdcwcs load_diags start_fpp wcs wcs_dual

all clean cleandir depend etc install lint:

.ifndef DESTDIR
distribution-etc-root-var distribution distrib-dirs snapshot:
	@echo setenv DESTDIR before doing that!
	@false
.else
distribution-etc-root-var: distrib-dirs
	install -c -o ${BINOWN} -g ${BINGRP} -m 644 ${BIN1} ${DESTDIR}/etc
	cat etc.${MACHINE}/ttys ttys.pty > ${DESTDIR}/etc/ttys && \
	    chown ${BINOWN} ${DESTDIR}/etc/ttys && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/ttys && \
	    chmod 644 ${DESTDIR}/etc/ttys
	cat fbtab.head etc.${MACHINE}/fbtab fbtab.tail > ${DESTDIR}/etc/fbtab && \
	    chown ${BINOWN} ${DESTDIR}/etc/fbtab && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/fbtab && \
	    chmod 644 ${DESTDIR}/etc/fbtab
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 ${BIN2} ${DESTDIR}/etc
	install -c -o root -g wheel -m 600 crontab ${DESTDIR}/var/cron/tabs/root
	install -c -o root -g wheel -m 600 master.passwd ${DESTDIR}/etc
	pwd_mkdb -p -d ${DESTDIR}/etc /etc/master.passwd
	install -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	     MAKEDEV.local etc.${MACHINE}/MAKEDEV ${DESTDIR}/dev
	(cd root; \
		install -c -o root -g wheel -m 644 dot.cshrc \
		    ${DESTDIR}/root/.cshrc; \
		install -c -o root -g wheel -m 644 dot.klogin \
		    ${DESTDIR}/root/.klogin; \
		install -c -o root -g wheel -m 644 dot.login \
		    ${DESTDIR}/root/.login; \
		install -c -o root -g wheel -m 644 dot.profile \
		    ${DESTDIR}/root/.profile; \
		rm -f ${DESTDIR}/.cshrc ${DESTDIR}/.profile; \
		ln ${DESTDIR}/root/.cshrc ${DESTDIR}/.cshrc; \
		ln ${DESTDIR}/root/.profile ${DESTDIR}/.profile)
	(cd kerberosIV; \
		install -c -o root -g wheel -m 644 README \
		    ${DESTDIR}/etc/kerberosIV; \
		install -c -o root -g wheel -m 644 krb.conf \
		    ${DESTDIR}/etc/kerberosIV; \
		install -c -o root -g wheel -m 444 krb.realms \
		    ${DESTDIR}/etc/kerberosIV)
	(cd amd; \
		install -c -o root -g wheel -m 644 master.sample \
		    ${DESTDIR}/etc/amd)
	(cd mtree; \
		install -c -o root -g wheel -m 600 special \
		    ${DESTDIR}/etc/mtree; \
		install -c -o root -g wheel -m 444 4.4BSD.dist \
		    ${DESTDIR}/etc/mtree)
	(cd ppp; \
		install -c -o root -g wheel -m 600 chap-secrets \
		    ${DESTDIR}/etc/ppp; \
		install -c -o root -g wheel -m 600 options \
		    ${DESTDIR}/etc/ppp; \
		install -c -o root -g wheel -m 600 options.leaf \
		    ${DESTDIR}/etc/ppp; \
		install -c -o root -g wheel -m 600 options.sample \
		    ${DESTDIR}/etc/ppp; \
		install -c -o root -g wheel -m 600 chatscript.sample \
		    ${DESTDIR}/etc/ppp; \
		install -c -o root -g wheel -m 600 pap-secrets \
		    ${DESTDIR}/etc/ppp)
	cd namedb; install -c -o ${BINOWN} -g ${BINGRP} -m 644 ${NAMEDB} \
	    ${DESTDIR}/etc/namedb
	/bin/rm -f ${DESTDIR}/etc/localtime
	ln -s ${TZDIR}/${LOCALTIME} ${DESTDIR}/etc/localtime
	/bin/rm -f ${DESTDIR}/etc/rmt
	ln -s /usr/sbin/rmt ${DESTDIR}/etc/rmt
	install -c -o root -g wheel -m 644 minfree \
		${DESTDIR}/var/crash
	install -c -o ${BINOWN} -g operator -m 664 /dev/null \
		${DESTDIR}/etc/dumpdates
	install -c -o ${BINOWN} -g operator -m 644 /dev/null \
		${DESTDIR}/etc/skeykeys
	install -c -o root -g wheel -m 600 /dev/null \
		${DESTDIR}/var/cron/log
	install -c -o nobody -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/db/locate.database
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/authlog
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/daemon
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/lastlog
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/lpd-errs
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/maillog
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/messages
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/ftpd
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/secure
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/wtmp
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/log/xferlog
	install -c -o daemon -g staff -m 664 /dev/null \
		${DESTDIR}/var/msgs/bounds
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
		${DESTDIR}/var/run/utmp
	(cd etc.${MACHINE}; install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    fstab.* ${DESTDIR}/etc)
	(cd ${DESTDIR}/dev; ./MAKEDEV all)
.if ${MACHINE} == "tahoe"
	(cd etc.tahoe; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${WCS1} \
	    ${DESTDIR}/)
.endif
.if ${MACHINE} == "vax"
	(cd etc.vax; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${PCS} \
	    ${DESTDIR}/)
.endif
	(cd ../usr.sbin/sendmail/cf/cf; ${MAKE} distribution)

distribution: distribution-etc-root-var
	(cd ..; ${MAKE} install)

hcx9-distribution:
	(cd etc.tahoe; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${WCS2} \
	    ${DESTDIR}/)

distrib-dirs:
	install -d -o root -g wheel -m 755 ${DESTDIR}
	-mtree -def mtree/4.4BSD.dist -p ${DESTDIR}/ -u
	if [ ! -f ${DESTDIR}/usr/src ]; then \
		install -d -o root -g wsrc -m 775 ${DESTDIR}/usr/src; \
	fi
	cd ${DESTDIR}; rm -f sys; ln -s usr/src/sys sys

snapshot: distribution snap_pre snap_tar snap_md
	cd ${DESTDIR}/snapshot && cksum * > CKSUMS

snap_pre:
	/bin/rm -rf ${DESTDIR}/snapshot
	install -d -o root -g wheel -m 755 ${DESTDIR}/snapshot

snap_tar:
	cd ${DESTDIR} && tar cf - bin | gzip -9 > snapshot/bin.tar.gz
	cd ${DESTDIR} && tar cf - dev | gzip -9 > snapshot/dev.tar.gz
	cd ${DESTDIR} && tar cf - .profile .cshrc etc mnt root sys tmp | \
	    gzip -9 > snapshot/etc.tar.gz
	cd ${DESTDIR} && tar cf - sbin | gzip -9 > snapshot/sbin.tar.gz
	cd ${DESTDIR} && tar cf - usr/bin | gzip -9 > snapshot/usr.bin.tar.gz
	cd ${DESTDIR} && tar cf - usr/games | gzip -9 > \
	    snapshot/usr.games.tar.gz
	cd ${DESTDIR} && tar cf - usr/include | gzip -9 > \
	    snapshot/usr.include.tar.gz
	cd ${DESTDIR} && tar cf - usr/lib | gzip -9 > snapshot/usr.lib.tar.gz
	cd ${DESTDIR} && tar cf - usr/libexec | gzip -9 > \
	    snapshot/usr.libexec.tar.gz
	cd ${DESTDIR} && tar cf - usr/mdec usr/libdata usr/local usr/src \
	    usr/obj | gzip -9 > snapshot/usr.misc.tar.gz
	cd ${DESTDIR} && tar cf - usr/sbin | gzip -9 > snapshot/usr.sbin.tar.gz
	cd ${DESTDIR} && tar cf - usr/share | gzip -9 > \
	    snapshot/usr.share.tar.gz
	cd ${DESTDIR} && tar cf - var | gzip -9 > snapshot/var.tar.gz

snap_md:
# nothing here -- look in the machine-dependent Makefile.inc

.endif	# DESTDIR check

.include <bsd.prog.mk>
