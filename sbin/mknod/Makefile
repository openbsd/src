#	$OpenBSD: Makefile,v 1.4 1999/04/18 19:40:41 millert Exp $

PROG=	mknod
MAN=	mknod.8 mkfifo.1
LINKS=	${BINDIR}/mknod ${BINDIR}/mkfifo

.include <bsd.prog.mk>

beforeinstall:
	ln -f -s ../../sbin/mknod ${DESTDIR}/usr/bin/mkfifo
