#	$OpenBSD: Makefile,v 1.3 1996/08/16 22:00:12 michaels Exp $
#
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $Id: Makefile,v 1.3 1996/08/16 22:00:12 michaels Exp $

PROG=	locate
MAN= locate.1 locate.updatedb.8
SCRIPTS= updatedb mklocatedb concatdb
MLINKS+= locate.updatedb.8 updatedb.8

beforeinstall:
.for script in ${SCRIPTS}
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
    	${.CURDIR}/${script}.sh ${DESTDIR}${LIBEXECDIR}/locate.${script}
.endfor

# only /usr/src/etc/Makefile install files in /etc
#	${INSTALL} -c -o root -g wheel -m 644 \
#		${.CURDIR}/locate.rc ${DESTDIR}/etc

.include "../../Makefile.inc"
.include <bsd.prog.mk>
