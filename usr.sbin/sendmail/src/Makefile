#	$OpenBSD: Makefile,v 1.17 1998/07/12 17:54:16 millert Exp $
#
#  OpenBSD Makefile
#
#

.include <bsd.own.mk>

PROG=	sendmail

# define the database format to use for aliases et al.  Can be -DNEWDB (for
# the new BSD database package -- this is preferred) or -DNDBM for the NDBM
# database package.  The old putrescent V7 DBM package is no longer
# supported.
# You can define both NEWDB and NDBM during a transition period; old
# databases are read, but the new format will be used on any rebuilds.  On
# really gnarly systems, you can set this to null; it will crawl like a high
# spiral snail, but it will work.
DBMDEF=	-DNEWDB

.if (${YP} == "yes")
DBMDEF+=-DNIS
.endif

CFLAGS+=-I${.CURDIR} ${DBMDEF} -DNETISO

.if (${TCP_WRAPPERS} == "yes")
CFLAGS+=-DTCPWRAPPERS

DPADD=	${LIBWRAP}
LDADD=	-lwrap
.endif

SRCS=	alias.c arpadate.c clock.c collect.c conf.c convtime.c daemon.c \
	deliver.c domain.c envelope.c err.c headers.c macro.c main.c map.c \
	mci.c mime.c parseaddr.c queue.c readcf.c recipient.c safefile.c \
	savemail.c srvrsmtp.c stab.c stats.c sysexits.c trace.c udb.c \
	usersmtp.c util.c version.c
MAN=	aliases.5 mailq.1 newaliases.1 sendmail.8
LINKS=	${BINDIR}/sendmail /usr/bin/newaliases \
	${BINDIR}/sendmail /usr/bin/mailq \
	${BINDIR}/sendmail /usr/bin/hoststat \
	${BINDIR}/sendmail /usr/bin/purgestat
BINOWN=	root
BINGRP=	bin
BINMODE=4555

beforeinstall:
#	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
#	    ${DESTDIR}/etc/sendmail.fc
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/sendmail.st
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/sendmail.hf ${DESTDIR}/usr/share/misc

.include "../../Makefile.inc"
.include <bsd.prog.mk>
