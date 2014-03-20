#	$OpenBSD: Makefile,v 1.14 2014/03/20 22:03:56 tedu Exp $

PROG=	md5
SRCS=	crc.c md5.c
MAN=	cksum.1 md5.1
LINKS=	${BINDIR}/md5 ${BINDIR}/sha1 \
	${BINDIR}/md5 ${BINDIR}/sha256 \
	${BINDIR}/md5 ${BINDIR}/sha512 \
	${BINDIR}/md5 ${BINDIR}/cksum
MLINKS= md5.1 sha1.1
MLINKS+= md5.1 sha256.1
MLINKS+= md5.1 sha512.1

CPPFLAGS+= -I${.CURDIR}
COPTS+=	-Wall -Wconversion -Wmissing-prototypes

.include <bsd.prog.mk>
