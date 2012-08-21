#	$OpenBSD: Makefile,v 1.3 2012/08/21 14:46:19 pascal Exp $

.include "${.CURDIR}/../Makefile.inc"

MAN=	mkboot.8

.if ${MACHINE} == "socppc"
PROG=	mkboot
DPADD=	${LIBZ}
LDADD=	-lz
NOPIE=
.else
NOPROG=
.endif

.include <bsd.prog.mk>
