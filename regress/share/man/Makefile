#	$OpenBSD: Makefile,v 1.5 2000/04/01 08:05:05 deraadt Exp $

NOPROG=
NOMAN=

regress: cfman no3man scatman
	perl cfman /usr/share/man
	perl no3man /usr/share/man
	perl scatman /usr/share/man

install:

.include <bsd.prog.mk>

