# Makefile for uusched
# $Id: Makefile,v 1.2 1997/04/27 20:55:53 millert Exp $

BINDIR=		$(bindir)

NOPROG=
CLEANFILES=	uusched

all:	${CLEANFILES}

uusched:	uusched.in
	sed -e "s|@BINDIR@|$(bindir)|g" -e "s|@SBINDIR@|$(sbindir)|g" \
		$(.CURDIR)/uusched.in > $(.TARGET)

install:
	install ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    uusched ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
