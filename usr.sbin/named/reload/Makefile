#	$OpenBSD: Makefile,v 1.6 1997/04/27 20:57:04 millert Exp $
#	$NetBSD: Makefile,v 1.1 1996/02/02 15:30:32 mrg Exp $
# from: $Id: Makefile,v 8.1 1994/12/15 06:23:46 vixie Exp 

.PATH:	${.CURDIR}/../man

all:	named.reload
CLEANFILES+= named.reload
MAN=	named.reload.8

realinstall: named.reload
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    named.reload ${DESTDIR}${BINDIR}/named.reload

named.reload:	named.reload.sh Makefile ${.CURDIR}/../Makefile.inc
	sed -e "s|%DESTSBIN%|${BINDIR}|" \
	    < ${.CURDIR}/named.reload.sh > named.reload
	chmod +x named.reload

.include <bsd.prog.mk>
.include "../../Makefile.inc"
