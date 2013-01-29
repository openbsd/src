#	$OpenBSD: Makefile,v 1.10 2013/01/29 11:08:55 espie Exp $

MAN=makewhatis.8
NOPROG=
PACKAGES= \
	OpenBSD/Makewhatis.pm \
	OpenBSD/Makewhatis/Check.pm \
	OpenBSD/Makewhatis/Find.pm \
	OpenBSD/Makewhatis/Formated.pm \
	OpenBSD/Makewhatis/Subject.pm \
	OpenBSD/Makewhatis/Unformated.pm \
	OpenBSD/Makewhatis/Whatis.pm

SCRIPTS= \
	makewhatis

LIBBASE=/usr/libdata/perl5

realinstall: 
.for i in ${PACKAGES}
	${INSTALL} -d -o ${LIBOWN} -g ${LIBGRP} -m ${DIRMODE} \
		${DESTDIR}${LIBBASE}/${i:H}
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
		${.CURDIR}/$i ${DESTDIR}${LIBBASE}/$i
.endfor
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/makewhatis ${DESTDIR}${BINDIR}/makewhatis

.include <bsd.prog.mk>
