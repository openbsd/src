#	$OpenBSD: Makefile,v 1.4 1997/02/10 12:33:49 mickey Exp $
#	$NetBSD: Makefile,v 1.6 1995/03/26 09:44:29 glass Exp $

PROG=	compress
SRCS=	compress.c zopen.c
MAN=	compress.1 zopen.3
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress
MLINKS= compress.1 uncompress.1
#MLINKS+=compress.1 zcat.1

#afterinstall:
#	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
#	    ${.CURDIR}/zcat.sh ${DESTDIR}/usr/bin/zcat

.include <bsd.prog.mk>
