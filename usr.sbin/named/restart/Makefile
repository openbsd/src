#	$NetBSD: Makefile,v 1.1 1996/02/02 15:30:35 mrg Exp $
# from: $Id: Makefile,v 8.1 1994/12/15 06:23:47 vixie Exp 

.PATH:	${.CURDIR}/../named \
	${.CURDIR}/../man

all: named.restart named.restart.cat8

CLEANFILES+= named.restart
MAN=	named.restart.8

realinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    named.restart ${DESTDIR}${BINDIR}/named.restart

named.restart:	named.restart.sh Makefile ${.CURDIR}/../Makefile.inc
	sed -e "s|%DESTSBIN%|${BINDIR}|" \
	    < ${.CURDIR}/named.restart.sh > named.restart
	chmod +x named.restart

.include <bsd.prog.mk>
.include "../../Makefile.inc"
