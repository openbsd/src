#	$OpenBSD: Makefile,v 1.5 2002/05/11 18:41:20 millert Exp $

PROG=	at
SRCS=	at.c panic.c parsetime.c perm.c
LINKS=	${BINDIR}/at ${BINDIR}/atq \
	${BINDIR}/at ${BINDIR}/atrm \
	${BINDIR}/at ${BINDIR}/batch
MLINKS=	at.1 batch.1 \
	at.1 atq.1 \
	at.1 atrm.1

BINGRP=	crontab
BINMODE= 2555

afterinstall:
	test -f ${DESTDIR}/var/at/.SEQ || touch ${DESTDIR}/var/at/.SEQ
	chown ${BINOWN}:${BINGRP} ${DESTDIR}/var/at/.SEQ; \
	chmod 0660 ${DESTDIR}/var/at/.SEQ; \

.include <bsd.prog.mk>
