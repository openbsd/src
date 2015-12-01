#	$OpenBSD: Makefile,v 1.14 2015/12/01 15:33:17 deraadt Exp $

PROG=		installboot
SRCS=		installboot.c util.c
MAN=		installboot.8

CPPFLAGS=	-I${.CURDIR}
LDADD=		-lutil
DPADD=		${LIBUTIL}

LDSTATIC=	${STATIC}

.if ${MACHINE} == "alpha"
SRCS += stubs.c
.endif

.if ${MACHINE} == "amd64"
CFLAGS += -DSOFTRAID
SRCS += i386_installboot.c
SRCS += i386_nlist.c
SRCS += i386_softraid.c
.endif

.if ${MACHINE} == "armish"
SRCS += stubs.c
.endif

.if ${MACHINE} == "armv7"
SRCS += stubs.c
.endif

.if ${MACHINE} == "hppa"
CFLAGS += -DBOOTSTRAP
SRCS += hppa_installboot.c
.endif

.if ${MACHINE} == "hppa64"
CFLAGS += -DBOOTSTRAP
SRCS += hppa64_installboot.c
.endif

.if ${MACHINE} == "i386"
CFLAGS += -DSOFTRAID
SRCS += i386_installboot.c
SRCS += i386_nlist.c
SRCS += i386_softraid.c
.endif

.if ${MACHINE} == "landisk"
CFLAGS += -DBOOTSTRAP
SRCS += landisk_installboot.c
.endif

.if ${MACHINE} == "loongson"
SRCS += stubs.c
.endif

.if ${MACHINE} == "luna88k"
SRCS += stubs.c
.endif

.if ${MACHINE} == "macppc"
SRCS += stubs.c
.endif

.if ${MACHINE} == "octeon"
SRCS += stubs.c
.endif

.if ${MACHINE} == "sgi"
SRCS += stubs.c
.endif

.if ${MACHINE} == "socppc"
SRCS += stubs.c
.endif

.if ${MACHINE} == "sparc"
SRCS += stubs.c
.endif

.if ${MACHINE} == "sparc64"
CFLAGS += -DSOFTRAID
SRCS += sparc64_installboot.c
SRCS += sparc64_softraid.c
.endif

.if ${MACHINE} == "vax"
CFLAGS += -DBOOTSTRAP
SRCS += vax_installboot.c
.endif

.if ${MACHINE} == "zaurus"
SRCS += stubs.c
.endif

.if !empty(CFLAGS:M-DBOOTSTRAP)
SRCS += bootstrap.c
.endif

.if !empty(CFLAGS:M-DSOFTRAID)
SRCS += softraid.c
.endif

.include <bsd.prog.mk>
