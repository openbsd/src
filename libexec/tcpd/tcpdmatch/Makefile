#	$OpenBSD: Makefile,v 1.1 1997/02/26 06:17:10 downsj Exp $

.PATH:	${.CURDIR}/../tcpdchk
CFLAGS+=-I${.CURDIR}/../tcpdchk

PROG=	tcpdmatch
MAN=	tcpdmatch.8

SRCS=	inetcf.c scaffold.c tcpdmatch.c

DPADD=	${LIBWRAP}
LDADD=	-lwrap

BINDIR=	/usr/sbin

.include <bsd.prog.mk>
