# $OpenBSD: Makefile,v 1.6 2012/07/13 11:56:12 espie Exp $

.include <bsd.own.mk>

MAN=libtool.1
NOPROG= 

PACKAGES= \
    LT/Archive.pm \
    LT/Exec.pm \
    LT/Getopt.pm \
    LT/LaFile.pm \
    LT/LaLoFile.pm \
    LT/Library.pm \
    LT/LoFile.pm \
    LT/Mode/Compile.pm \
    LT/Mode/Install.pm \
    LT/Mode/Link.pm \
    LT/Mode/Link/Program.pm \
    LT/Mode/Link/Library.pm \
    LT/Program.pm \
    LT/Trace.pm \
    LT/Util.pm

LIBBASE=/usr/libdata/perl5

realinstall: 
.for d in LT LT/Mode LT/Mode/Link
	${INSTALL} -d -o ${LIBOWN} -g ${LIBGRP} -m ${DIRMODE} \
		${DESTDIR}${LIBBASE}/$d
.endfor
.for i in ${PACKAGES}
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
		${.CURDIR}/$i ${DESTDIR}${LIBBASE}/$i
.endfor
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/libtool ${DESTDIR}${BINDIR}/libtool

.include <bsd.prog.mk>
