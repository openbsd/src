#	$OpenBSD: Makefile,v 1.20 1999/01/01 20:33:35 millert Exp $
#
#  OpenBSD Makefile
#
#

.include <bsd.own.mk>

PROG=	sendmail

# define the database mechanisms available for map & alias lookups:
#	-DNDBM -- use new DBM
#	-DNEWDB -- use new Berkeley DB
#	-DNIS -- include NIS support
# The really old (V7) DBM library is no longer supported.
# See README for a description of how these flags interact.
#
MAPDEF=	-DNEWDB -DMAP_REGEX

.if (${YP} == "yes")
MAPDEF+=-DNIS
.endif

# environment definitions (e.g., -DNETISO)
ENVDEF= -DNETISO -D_FFR_MAX_MIME_HEADER_LENGTH=1

.if (${TCP_WRAPPERS} == "yes")
ENVDEF+=-DTCPWRAPPERS
DPADD=	${LIBWRAP}
LDADD=	-lwrap
.endif

CFLAGS+=-I${.CURDIR} ${MAPDEF} ${ENVDEF}

SRCS=	alias.c arpadate.c clock.c collect.c conf.c control.c convtime.c \
	daemon.c deliver.c domain.c envelope.c err.c headers.c macro.c main.c \
	map.c mci.c mime.c parseaddr.c queue.c readcf.c recipient.c safefile.c \
	savemail.c snprintf.c srvrsmtp.c stab.c stats.c sysexits.c trace.c \
	udb.c usersmtp.c util.c version.c
MAN=	aliases.5 mailq.1 newaliases.1 sendmail.8
LINKS=	${BINDIR}/sendmail /usr/bin/newaliases \
	${BINDIR}/sendmail /usr/bin/mailq \
	${BINDIR}/sendmail /usr/bin/hoststat \
	${BINDIR}/sendmail /usr/bin/purgestat
BINOWN=	root
BINGRP=	bin
BINMODE=4555

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/sendmail.st
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/sendmail.hf ${DESTDIR}/usr/share/misc

.include "../../Makefile.inc"
.include <bsd.prog.mk>
