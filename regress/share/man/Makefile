#	$OpenBSD: Makefile,v 1.4 2000/04/01 06:01:56 deraadt Exp $

NOPROG=
NOMAN=

regress: cfman no3man scatman
	perl cfman /usr/share/man
	perl no3man /usr/share/man
	perl scatman /usr/share/man

obj depend clean cleandir:

install:

