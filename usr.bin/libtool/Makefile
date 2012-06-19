# $OpenBSD: Makefile,v 1.1 2012/06/19 09:30:44 espie Exp $

.include <bsd.own.mk>

MAN=libtool.1
NOPROG= 

PACKAGES= \
    LT/Archive.pm \
    LT/Exec.pm \
    LT/LaFile.pm \
    LT/LaLoFile.pm \
    LT/Library.pm \
    LT/LoFile.pm \
    LT/Parser.pm \
    LT/Program.pm \
    LT/Trace.pm \
    LT/Util.pm

LIBBASE=/usr/libdata/perl5

realinstall: 
	${INSTALL} -d -o ${LIBOWN} -g ${LIBGRP} -m ${DIRMODE} \
		${DESTDIR}${LIBBASE}/LT
.for i in ${PACKAGES}
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
		${.CURDIR}/$i ${DESTDIR}${LIBBASE}/$i
.endfor
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/libtool ${DESTDIR}${BINDIR}/libtool

.include <bsd.prog.mk>
