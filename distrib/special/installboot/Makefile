#	$OpenBSD: Makefile,v 1.11 2019/05/11 07:18:15 deraadt Exp $

.PATH: ${.CURDIR}/../../../usr.sbin/installboot

PROG=		installboot
SRCS=		installboot.c util.c

CPPFLAGS=	-I${.CURDIR}/../../../usr.sbin/installboot
LDADD=		-lutil
DPADD=		${LIBUTIL}

.if ${MACHINE} == "alpha"
SRCS += stubs.c
.endif

.if ${MACHINE} == "amd64"
CFLAGS += -DSOFTRAID
SRCS += i386_installboot.c
SRCS += i386_nlist.c
SRCS += i386_softraid.c
.endif

.if ${MACHINE} == "arm64"
SRCS += stubs.c
.endif

.if ${MACHINE} == "armv7"
SRCS += stubs.c
.endif

.if ${MACHINE} == "hppa"
CFLAGS += -DBOOTSTRAP
SRCS += hppa_installboot.c
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

.if ${MACHINE} == "sparc64"
CFLAGS += -DSOFTRAID
SRCS += sparc64_installboot.c
SRCS += sparc64_softraid.c
.endif

.if !empty(CFLAGS:M-DBOOTSTRAP)
SRCS += bootstrap.c
.endif

.if !empty(CFLAGS:M-DSOFTRAID)
SRCS += softraid.c
.endif

.include <bsd.prog.mk>
