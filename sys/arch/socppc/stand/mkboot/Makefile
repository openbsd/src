#	$OpenBSD: Makefile,v 1.2 2008/06/09 15:01:56 deraadt Exp $

.include "${.CURDIR}/../Makefile.inc"

MAN=	mkboot.8

.if ${MACHINE} == "socppc"
PROG=	mkboot
DPADD=	${LIBZ}
LDADD=	-lz
.else
NOPROG=
.endif

.include <bsd.prog.mk>
