#	$OpenBSD: Makefile,v 1.8 1996/12/08 14:32:28 downsj Exp $
#
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $Id: Makefile,v 1.8 1996/12/08 14:32:28 downsj Exp $

PROG=	locate
SRCS=	util.c locate.c
CFLAGS+= -I${.CURDIR} -DMMAP # -DDEBUG (print time) -O2 (10% faster)
MAN=	locate.1	locate.updatedb.8
SCRIPTS= updatedb mklocatedb concatdb
MLINKS+= locate.updatedb.8 updatedb.8


beforeinstall:
	-@for i in ${SCRIPTS}; do \
			${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m \
				${BINMODE} ${.CURDIR}/$$i.sh \
				${DESTDIR}${LIBEXECDIR}/locate.$$i;\
	done

# only /usr/src/etc/Makefile install files in /etc
#	${INSTALL} ${COPY} -o root -g wheel -m 644 \
#		${.CURDIR}/locate.rc ${DESTDIR}/etc

.include "../../Makefile.inc"
.include "../Makefile.inc"
.include <bsd.prog.mk>
