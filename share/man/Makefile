#	from: @(#)Makefile	5.9 (Berkeley) 7/1/91
#	$Id: Makefile,v 1.4 1999/07/09 21:41:46 hugh Exp $

SUBDIR=	man1 man3 man4 man5 man6 man7 man8 man9

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 makewhatis.sed \
	    ${DESTDIR}/usr/share/man/makewhatis.sed
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 man0/COPYRIGHT \
	    ${DESTDIR}/usr/share/man/COPYRIGHT

makedb:
	/usr/libexec/makewhatis ${DESTDIR}/usr/share/man

.include <bsd.subdir.mk>
