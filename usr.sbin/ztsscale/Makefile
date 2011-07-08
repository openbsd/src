#	$OpenBSD: Makefile,v 1.3 2011/07/08 19:53:22 martynas Exp $

.if ${MACHINE} == "zaurus"
SRCS=	ztsscale.c

LDADD=	-lm
DPADD=	${LIBM}

PROG=	ztsscale
.else
NOPROG=yes
.endif

MAN=	ztsscale.8
MANSUBDIR=zaurus

.include <bsd.prog.mk>
