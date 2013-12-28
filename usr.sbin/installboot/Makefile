#	$OpenBSD: Makefile,v 1.5 2013/12/28 12:03:57 jsing Exp $

NOMAN=

.if (${MACHINE} == "amd64" || ${MACHINE} == "hppa" || ${MACHINE} == "i386" || \
     ${MACHINE} == "vax")

PROG=		installboot

CPPFLAGS=	-I${.CURDIR}
LDADD=		-lutil
DPADD=		${LIBUTIL}

LDSTATIC=	-static

SRCS += installboot.c

.include "${.CURDIR}/${MACHINE}/Makefile.inc"

.if !empty(CFLAGS:M-DBOOTSTRAP)
SRCS += bootstrap.c
.endif

.if !empty(CFLAGS:M-DSOFTRAID)
SRCS += softraid.c
.endif

.else
NOPROG=
.endif

.include <bsd.prog.mk>
