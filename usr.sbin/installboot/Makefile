#	$OpenBSD: Makefile,v 1.10 2014/01/18 02:47:27 jsing Exp $

PROG=		installboot
SRCS=		installboot.c util.c
MAN=		installboot.8

CPPFLAGS=	-I${.CURDIR}
LDADD=		-lutil
DPADD=		${LIBUTIL}

LDSTATIC=	-static

.include "${.CURDIR}/${MACHINE}/Makefile.inc"

.if !empty(CFLAGS:M-DBOOTSTRAP)
SRCS += bootstrap.c
.endif

.if !empty(CFLAGS:M-DSOFTRAID)
SRCS += softraid.c
.endif

.include <bsd.prog.mk>
