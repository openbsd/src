#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:20 downsj Exp $
#	$NetBSD: Makefile,v 1.6 1995/03/26 09:44:29 glass Exp $

PROG=	compress
SRCS=	compress.c zopen.c
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress
MLINKS= compress.1 uncompress.1
#MLINKS+=compress.1 zcat.1

#afterinstall:
#	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
#	    ${.CURDIR}/zcat.sh ${DESTDIR}/usr/bin/zcat

.include <bsd.prog.mk>
