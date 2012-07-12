# $OpenBSD: Makefile,v 1.5 2012/07/12 17:07:12 espie Exp $

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
    LT/Linker.pm \
    LT/LoFile.pm \
    LT/Mode/Compile.pm \
    LT/Mode/Install.pm \
    LT/Mode/Link.pm \
    LT/Program.pm \
    LT/Trace.pm \
    LT/Util.pm

LIBBASE=/usr/libdata/perl5

realinstall: 
	${INSTALL} -d -o ${LIBOWN} -g ${LIBGRP} -m ${DIRMODE} \
		${DESTDIR}${LIBBASE}/LT
	${INSTALL} -d -o ${LIBOWN} -g ${LIBGRP} -m ${DIRMODE} \
		${DESTDIR}${LIBBASE}/LT/Mode
.for i in ${PACKAGES}
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
		${.CURDIR}/$i ${DESTDIR}${LIBBASE}/$i
.endfor
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/libtool ${DESTDIR}${BINDIR}/libtool

.include <bsd.prog.mk>
