#	$OpenBSD: Makefile,v 1.5 1997/04/27 20:56:41 millert Exp $
#	$NetBSD: Makefile,v 1.6 1995/03/26 09:44:29 glass Exp $

PROG=	compress
SRCS=	compress.c zopen.c
MAN=	compress.1 zopen.3
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress
MLINKS= compress.1 uncompress.1
#MLINKS+=compress.1 zcat.1

#afterinstall:
#	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
#	    ${.CURDIR}/zcat.sh ${DESTDIR}/usr/bin/zcat

.include <bsd.prog.mk>
