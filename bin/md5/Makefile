#	$OpenBSD: Makefile,v 1.8 2004/05/02 17:53:29 millert Exp $

PROG=	md5
SRCS=	crc.c md5.c
MAN=	cksum.1 md5.1 sha1.1 rmd160.1
LINKS=	${BINDIR}/md5 ${BINDIR}/sha1 \
	${BINDIR}/md5 ${BINDIR}/rmd160 \
	${BINDIR}/md5 ${BINDIR}/cksum \
	${BINDIR}/md5 ${BINDIR}/sum

CPPFLAGS+= -I${.CURDIR}
COPTS+=	-ansi -Wall -Wconversion -Wmissing-prototypes

.include <bsd.prog.mk>
