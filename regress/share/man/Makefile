#	$OpenBSD: Makefile,v 1.7 2002/01/01 23:05:30 art Exp $

REGRESSTARGETS=do-cfman do-no3man do-scatman
do-cfman:
	perl ${.CURDIR}/cfman /usr/share/man
do-no3man:
	perl ${.CURDIR}/no3man /usr/share/man
do-scatman:
	perl ${.CURDIR}/scatman /usr/share/man

.include <bsd.regress.mk>

