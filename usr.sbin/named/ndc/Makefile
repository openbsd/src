#	$NetBSD: Makefile,v 1.1 1996/02/02 15:29:48 mrg Exp $
# from: $Id: Makefile,v 8.1 1994/12/15 06:23:47 vixie Exp 

.PATH:	${.CURDIR}/../named \
	${.CURDIR}/../man

CLEANFILES+= ndc
MAN=	ndc.8

realinstall: ndc
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ndc ${DESTDIR}${BINDIR}/ndc

ndc: ndc.sh Makefile ${.CURDIR}/../Makefile.inc
	sed -e "s|%PIDDIR%|${PIDDIR}|" \
	    -e "s|%PS%|${PS}|" \
	    -e "s|%DESTSBIN%|${BINDIR}|" \
	    -e "s|%IOT%|${IOT}|" \
	    < ${.CURDIR}/ndc.sh > ndc
	chmod +x ndc

.include "../../Makefile.inc"
.include <bsd.prog.mk>
