#	$OpenBSD: Makefile,v 1.6 2000/03/02 14:46:41 todd Exp $
#	from: @(#)Makefile	5.9 (Berkeley) 7/1/91

SUBDIR=	man1 man3 man4 man5 man6 man7 man8 man9

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 man0/COPYRIGHT \
	    ${DESTDIR}/usr/share/man/COPYRIGHT

makedb:
	/usr/libexec/makewhatis ${DESTDIR}/usr/share/man

.include <bsd.subdir.mk>
