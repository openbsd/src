#	$OpenBSD: Makefile,v 1.1 1997/02/26 06:17:06 downsj Exp $

PROG=	tcpdchk
MAN=	tcpdchk.8

SRCS=	inetcf.c scaffold.c tcpdchk.c

DPADD=	${LIBWRAP}
LDADD=	-lwrap

BINDIR=	/usr/sbin

.include <bsd.prog.mk>
