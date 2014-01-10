#	$OpenBSD: Makefile,v 1.13 2014/01/10 17:47:44 deraadt Exp $

PROG=	md5
SRCS=	crc.c md5.c
MAN=	cksum.1 md5.1
LINKS=	${BINDIR}/md5 ${BINDIR}/sha1 \
	${BINDIR}/md5 ${BINDIR}/sha256 \
	${BINDIR}/md5 ${BINDIR}/sha512 \
	${BINDIR}/md5 ${BINDIR}/cksum \
	${BINDIR}/md5 ${BINDIR}/sum
MLINKS= cksum.1 sum.1
MLINKS+= md5.1 sha1.1
MLINKS+= md5.1 sha256.1
MLINKS+= md5.1 sha512.1

CPPFLAGS+= -I${.CURDIR}
COPTS+=	-Wall -Wconversion -Wmissing-prototypes

.include <bsd.prog.mk>
