#	$OpenBSD: Makefile,v 1.6 1996/08/17 10:51:26 michaels Exp $
#
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $Id: Makefile,v 1.6 1996/08/17 10:51:26 michaels Exp $


PROG=	locate
MAN= locate.1 locate.updatedb.8
SCRIPTS= updatedb mklocatedb concatdb
MLINKS+= locate.updatedb.8 updatedb.8

beforeinstall:
	-@for i in $(SCRIPTS); do \
		install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		    ${.CURDIR}/$$i.sh ${DESTDIR}${LIBEXECDIR}/locate.$$i; \
	done

.include "../Makefile.inc"
.include "../../Makefile.inc"
.include <bsd.prog.mk>
