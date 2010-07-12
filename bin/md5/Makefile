#	$OpenBSD: Makefile,v 1.11 2010/07/12 23:16:55 tedu Exp $

PROG=	md5
SRCS=	crc.c md5.c
MAN=	cksum.1 md5.1 sha1.1 sha256.1
LINKS=	${BINDIR}/md5 ${BINDIR}/sha1 \
	${BINDIR}/md5 ${BINDIR}/sha256 \
	${BINDIR}/md5 ${BINDIR}/cksum \
	${BINDIR}/md5 ${BINDIR}/sum
MLINKS= cksum.1 sum.1

CPPFLAGS+= -I${.CURDIR}
COPTS+=	-Wall -Wconversion -Wmissing-prototypes

.include <bsd.prog.mk>
