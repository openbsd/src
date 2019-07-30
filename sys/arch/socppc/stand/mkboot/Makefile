#	$OpenBSD: Makefile,v 1.4 2012/09/02 17:22:53 pascal Exp $

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
