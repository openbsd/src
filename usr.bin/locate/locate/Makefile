#	$OpenBSD: Makefile,v 1.7 1996/09/15 16:50:37 michaels Exp $
#
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $Id: Makefile,v 1.7 1996/09/15 16:50:37 michaels Exp $

PROG=	locate
SRCS=	util.c locate.c
CFLAGS+= -I${.CURDIR} -DMMAP # -DDEBUG (print time) -O2 (10% faster)
MAN=	locate.1	locate.updatedb.8
SCRIPTS= updatedb mklocatedb concatdb
MLINKS+= locate.updatedb.8 updatedb.8


beforeinstall:
	-@for i in ${SCRIPTS}; do \
			install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
    		${.CURDIR}/$$i.sh ${DESTDIR}${LIBEXECDIR}/locate.$$i;\
	done

# only /usr/src/etc/Makefile install files in /etc
#	${INSTALL} -c -o root -g wheel -m 644 \
#		${.CURDIR}/locate.rc ${DESTDIR}/etc

.include "../../Makefile.inc"
.include "../Makefile.inc"
.include <bsd.prog.mk>
