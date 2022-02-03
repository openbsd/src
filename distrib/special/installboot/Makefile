#	$OpenBSD: Makefile,v 1.16 2022/02/03 10:27:33 visa Exp $

.PATH: ${.CURDIR}/../../../usr.sbin/installboot

PROG=		installboot
SRCS=		installboot.c util.c
MAN=		installboot.8

CPPFLAGS=	-I${.CURDIR}/../../../usr.sbin/installboot
LDADD=		-lutil
DPADD=		${LIBUTIL}

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
CFLAGS += -DSOFTRAID
SRCS += i386_installboot.c
SRCS += i386_nlist.c
SRCS += i386_softraid.c
.elif ${MACHINE} == "armv7" || ${MACHINE} == "arm64" || ${MACHINE} == "riscv64"
SRCS += efi_installboot.c
.elif ${MACHINE} == "hppa"
CFLAGS += -DBOOTSTRAP
SRCS += hppa_installboot.c
.elif ${MACHINE} == "landisk"
CFLAGS += -DBOOTSTRAP
SRCS += landisk_installboot.c
.elif ${MACHINE} == "loongson"
SRCS += loongson_installboot.c
.elif ${MACHINE} == "macppc"
SRCS += macppc_installboot.c
.elif ${MACHINE} == "powerpc64"
SRCS += powerpc64_installboot.c
.elif ${MACHINE} == "octeon"
SRCS += octeon_installboot.c
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
