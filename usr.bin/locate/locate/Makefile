#	$OpenBSD: Makefile,v 1.5 1996/08/17 09:37:45 michaels Exp $
#
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $Id: Makefile,v 1.5 1996/08/17 09:37:45 michaels Exp $

PROG=	locate
MAN= locate.1 locate.updatedb.8
SCRIPTS= updatedb mklocatedb concatdb
MLINKS+= locate.updatedb.8 updatedb.8
LIBEXECDIR=/usr/libexec

beforeinstall:
	-@for i in $(SCRIPTS); do \
		install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		    ${.CURDIR}/$$i.csh ${DESTDIR}${LIBEXECDIR}/locate.$$i; \
	done

.include <bsd.prog.mk>
