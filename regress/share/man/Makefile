#	$OpenBSD: Makefile,v 1.6 2000/04/02 18:47:50 deraadt Exp $

NOPROG=
NOMAN=

regress: cfman no3man scatman
	perl ${.CURDIR}/cfman /usr/share/man
	perl ${.CURDIR}/no3man /usr/share/man
	perl ${.CURDIR}/scatman /usr/share/man

install:

.include <bsd.prog.mk>

