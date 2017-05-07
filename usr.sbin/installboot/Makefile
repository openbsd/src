#	$OpenBSD: Makefile,v 1.20 2017/05/07 10:40:17 kettenis Exp $

PROG=		installboot
SRCS=		installboot.c util.c
MAN=		installboot.8

CPPFLAGS=	-I${.CURDIR}
LDADD=		-lutil
DPADD=		${LIBUTIL}

LDSTATIC=	${STATIC}

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
CFLAGS += -DSOFTRAID
SRCS += i386_installboot.c
SRCS += i386_nlist.c
SRCS += i386_softraid.c
.elif ${MACHINE} == "armv7" || ${MACHINE} == "arm64"
SRCS += armv7_installboot.c
.elif ${MACHINE} == "hppa"
CFLAGS += -DBOOTSTRAP
SRCS += hppa_installboot.c
.elif ${MACHINE} == "landisk"
CFLAGS += -DBOOTSTRAP
SRCS += landisk_installboot.c
.elif ${MACHINE} == "sparc64"
CFLAGS += -DSOFTRAID
SRCS += sparc64_installboot.c
SRCS += sparc64_softraid.c
.else
SRCS += stubs.c
.endif

.if !empty(CFLAGS:M-DBOOTSTRAP)
SRCS += bootstrap.c
.endif

.if !empty(CFLAGS:M-DSOFTRAID)
SRCS += softraid.c
.endif

.include <bsd.prog.mk>
