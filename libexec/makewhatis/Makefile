#	$OpenBSD: Makefile,v 1.7 2004/08/06 12:05:08 espie Exp $

MAN=makewhatis.8
PACKAGES= \
	OpenBSD/Makewhatis.pm \
	OpenBSD/Makewhatis/Check.pm \
	OpenBSD/Makewhatis/Find.pm \
	OpenBSD/Makewhatis/Formated.pm \
	OpenBSD/Makewhatis/Unformated.pm \
	OpenBSD/Makewhatis/Whatis.pm

SCRIPTS= \
	makewhatis

LIBBASE=/usr/libdata/perl5

# Nothing to build
depend:
all:

install: 
.for i in ${PACKAGES}
	${INSTALL} -d -o ${LIBOWN} -g ${LIBGRP} -m ${DIRMODE} \
		${DESTDIR}${LIBBASE}/${i:H}
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
		${.CURDIR}/$i ${DESTDIR}${LIBBASE}/$i
.endfor
.for i in ${SCRIPTS}
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/$i ${DESTDIR}${BINDIR}/$i
.endfor

clean:

.include <bsd.own.mk>

.if !defined(NOMAN)
install: maninstall
.include <bsd.man.mk>
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
