#	$OpenBSD: Makefile,v 1.8 2001/02/13 01:12:33 fgsch Exp $

PROG=	ipfstat
MAN=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c facpri.c common.c ifaddr.c
.PATH:  ${.CURDIR}/../../sbin/ipf
CFLAGS+=-I${.CURDIR}/../../sbin/ipf -DSTATETOP
DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>
