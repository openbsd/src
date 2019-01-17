#	$OpenBSD: Makefile,v 1.13 2019/01/17 06:15:44 tedu Exp $
#
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	locate
SRCS=	util.c locate.c
CFLAGS+= -I${.CURDIR}
MAN=	locate.1	locate.updatedb.8
SCRIPTS= updatedb mklocatedb


beforeinstall:
	-@for i in ${SCRIPTS}; do \
			${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m \
				${BINMODE} ${.CURDIR}/$$i.sh \
				${DESTDIR}${LIBEXECDIR}/locate.$$i;\
	done

# only /usr/src/etc/Makefile install files in /etc
#	${INSTALL} ${INSTALL_COPY} -o root -g wheel -m 644 \
#		${.CURDIR}/locate.rc ${DESTDIR}/etc

.include "../../Makefile.inc"
.include "../Makefile.inc"
.include <bsd.prog.mk>
