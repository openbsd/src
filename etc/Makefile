#	$OpenBSD: Makefile,v 1.206 2004/05/04 23:37:04 deraadt Exp $

TZDIR=		/usr/share/zoneinfo
LOCALTIME=	Canada/Mountain

NOOBJ=	oobj

.if exists(etc.${MACHINE}/Makefile.inc)
.include "etc.${MACHINE}/Makefile.inc"
.endif

# -rw-r--r--
BINOWN= root
BINGRP= wheel
BIN1=	changelist ccd.conf csh.cshrc csh.login csh.logout daily dhcpd.conf \
	dhcpd.interfaces exports ftpusers ftpchroot gettytab group hosts \
	hosts.lpd inetd.conf ksh.kshrc locate.rc man.conf monthly motd \
	mrouted.conf myname netstart networks newsyslog.conf phones printcap \
	protocols rbootd.conf rc rc.conf rc.local rc.securelevel rc.shutdown \
	remote rpc security services shells spamd.conf syslog.conf weekly \
	etc.${MACHINE}/disktab dhclient.conf mailer.conf

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "cats" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "i386" || ${MACHINE} == "luna88k" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "sparc64"
BIN1+=	wsconsctl.conf
.endif

# -rw-rw-r--
BIN2=	motd

NAMEDB=	localhost.rev localhost.v6.rev localhost.zone root.cache
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
	    chmod 644 ${DESTDIR}/etc/sysctl.conf
	cat fbtab.head etc.${MACHINE}/fbtab fbtab.tail > ${DESTDIR}/etc/fbtab && \
	    chown ${BINOWN} ${DESTDIR}/etc/fbtab && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/fbtab && \
	    chmod 644 ${DESTDIR}/etc/fbtab
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 ${BIN2} ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 hosts.equiv ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 crontab ${DESTDIR}/var/cron/tabs/root
	${INSTALL} -c -o root -g wheel -m 644 login.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 master.passwd ${DESTDIR}/etc
	pwd_mkdb -p -d ${DESTDIR}/etc /etc/master.passwd
	${INSTALL} -c -o root -g wheel -m 644 moduli ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 bgpd.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 pf.conf ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 644 pf.os ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 644 sensorsd.conf ${DESTDIR}/etc
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
		rm -f ${DESTDIR}/.cshrc ${DESTDIR}/.profile; \
		ln ${DESTDIR}/root/.cshrc ${DESTDIR}/.cshrc; \
		ln ${DESTDIR}/root/.profile ${DESTDIR}/.profile
	cd skel; \
		${INSTALL} -c -o root -g wheel -m 644 dot.cshrc \
		    ${DESTDIR}/etc/skel/.cshrc; \
		${INSTALL} -c -o root -g wheel -m 644 dot.login \
		    ${DESTDIR}/etc/skel/.login; \
		${INSTALL} -c -o root -g wheel -m 644 dot.mailrc \
		    ${DESTDIR}/etc/skel/.mailrc; \
		${INSTALL} -c -o root -g wheel -m 644 dot.profile \
		    ${DESTDIR}/etc/skel/.profile
	cd kerberosV; \
		${INSTALL} -c -o root -g wheel -m 644 README \
		    ${DESTDIR}/etc/kerberosV; \
		${INSTALL} -c -o root -g wheel -m 644 krb5.conf.example \
		    ${DESTDIR}/etc/kerberosV
	cd amd; \
		${INSTALL} -c -o root -g wheel -m 644 master.sample \
		    ${DESTDIR}/etc/amd
	cd mtree; \
		${INSTALL} -c -o root -g wheel -m 600 special \
		    ${DESTDIR}/etc/mtree; \
		${INSTALL} -c -o root -g wheel -m 444 4.4BSD.dist \
		    ${DESTDIR}/etc/mtree; \
		${INSTALL} -c -o root -g wheel -m 444 BSD.local.dist \
		    ${DESTDIR}/etc/mtree; \
		${INSTALL} -c -o root -g wheel -m 444 BSD.x11.dist \
		    ${DESTDIR}/etc/mtree
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
		    ${DESTDIR}/etc/systrace;
	cd bind; \
		${INSTALL} -c -o root -g named -m 640 named-simple.conf \
		    ${DESTDIR}/var/named/etc/named.conf; \
		${INSTALL} -c -o root -g named -m 640 named-*.conf \
		    ${DESTDIR}/var/named/etc; \
		${INSTALL} -c -o root -g wheel -m 644 db.localhost \
		    ${DESTDIR}/var/named/standard/localhost; \
		${INSTALL} -c -o root -g wheel -m 644 db.loopback \
		    ${DESTDIR}/var/named/standard/loopback; \
		${INSTALL} -c -o root -g wheel -m 644 db.loopback6.arpa \
		    ${DESTDIR}/var/named/standard/loopback6.arpa; \
		${INSTALL} -c -o root -g wheel -m 644 db.loopback6.int \
		    ${DESTDIR}/var/named/standard/loopback6.int; \
		${INSTALL} -c -o root -g wheel -m 644 root.hint \
		    ${DESTDIR}/var/named/standard; \
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
	cd ../usr.bin/mail && exec ${MAKE} distribution
	cd mail && exec ${MAKE} distribution
	${INSTALL} -c -o root -g wheel -m 600 root/root.mail \
		${DESTDIR}/var/mail/root
	${INSTALL} -c -o root -g wheel -m 440 ../usr.bin/sudo/sudoers \
		${DESTDIR}/etc/sudoers

distribution:
	exec ${SUDO} ${MAKE} distribution-etc-root-var
	cd .. && exec ${SUDO} ${MAKE} install

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
release: distribution snap_pre snap_md
	cd ${.CURDIR}/../distrib/notes && ${MAKE} && exec ${SUDO} ${MAKE} install
	cd ${.CURDIR}/../distrib/sets && exec ${SUDO} sh maketars ${OSrev}
	-cp ${DESTDIR}/snapshot/bsd* ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/*boot* ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/*BOOT* ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/cd*.iso ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/INSTALL.* ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/*.fs ${DESTDIR}/snapshot/*.fs.gz ${RELEASEDIR}
	-cd ${RELEASEDIR}; \
		md5 bsd!(*.gz) *boot* *BOOT* INSTALL.* *.fs *.iso *.gz *.tgz \
		    > MD5
	-cd ${RELEASEDIR}; \
		cksum bsd!(*.gz) *boot* *BOOT* INSTALL.* *.fs *.iso *.gz *.tgz \
		    > CKSUM
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
	${SUDO} /bin/rm -rf ${DESTDIR}/snapshot
	${SUDO} ${INSTALL} -d -o root -g wheel -m 755 ${DESTDIR}/snapshot

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

MAKEDEVARCHS+= alpha
MAKEDEVARCHS+= amd64
MAKEDEVARCHS+= cats
MAKEDEVARCHS+= hp300
MAKEDEVARCHS+= hppa
MAKEDEVARCHS+= i386
MAKEDEVARCHS+= luna88k
MAKEDEVARCHS+= mac68k
MAKEDEVARCHS+= macppc
MAKEDEVARCHS+= mvme68k
MAKEDEVARCHS+= mvme88k
#MAKEDEVARCHS+= mvmeppc
MAKEDEVARCHS+= sparc
MAKEDEVARCHS+= sparc64
MAKEDEVARCHS+= vax

clean:
	rm -f etc.${MACHINE}/MAKEDEV

cleandir:
	cd ${.CURDIR}; for m in ${MAKEDEVARCHS}; do \
	    ${MAKE} MACHINE=$$m clean; done

MAKEDEVSRC=	MAKEDEV.mi MAKEDEV.sub MAKEDEV.common
MAKEDEVDOC=	MAKEDEV.man MAKEDEV.mansub MAKEDEV.common
m4: etc.${M}/MAKEDEV
man: ${.CURDIR}/../share/man/man8/man.${M}/MAKEDEV.8

etc.${M}/MAKEDEV: ${MAKEDEVSRC} etc.${M}/MAKEDEV.md
	@echo "==> etc.${M}/MAKEDEV"
	m4 -DMACHINE=${M} MAKEDEV.mi > etc.${M}/.MAKEDEV.tmp && \
	    mv etc.${M}/.MAKEDEV.tmp etc.${M}/MAKEDEV || \
	    rm etc.${M}/.MAKEDEV.tmp

${.CURDIR}/../share/man/man8/man.${M}/MAKEDEV.8: ${MAKEDEVDOC} etc.${M}/MAKEDEV.md
	m4 -DMACHINE=${M} MAKEDEV.man > \
	    ${.CURDIR}/../share/man/man8/man8.${M}/MAKEDEV.8

allarchs: ${MAKEDEVSRC} ${MAKEDEVDOC}
	cd ${.CURDIR}; for m in ${MAKEDEVARCHS}; do \
	    ${MAKE} M=$$m m4 man; done

distrib:
	cd ${.CURDIR}/../distrib && \
	    ${MAKE} cleandir && ${MAKE} && exec ${SUDO} ${MAKE} install

DHSIZE=1024 1536 2048 3072 4096
update-moduli:
	( \
		echo '#    $$OpenBSD: Makefile,v 1.206 2004/05/04 23:37:04 deraadt Exp $$'; \
		echo '# Time Type Tests Tries Size Generator Modulus'; \
		( for i in ${DHSIZE}; do \
			ssh-keygen -b $$i -G /dev/stdout; \
		done) | \
		ssh-keygen -T /dev/stdout \
	) > moduli

.PHONY: distribution-etc-root-var distribution distrib-dirs \
	release snapshot allarchs snap_pre snap_tar snap_md m4
.include <bsd.prog.mk>
