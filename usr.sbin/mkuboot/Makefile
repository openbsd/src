#	$OpenBSD: Makefile,v 1.4 2017/01/30 06:46:13 jsg Exp $

.if ${MACHINE} == "arm64" || ${MACHINE} == "armv7

PROG=	mkuboot
SRCS=	mkuboot.c copy_elf32.c copy_elf64.c
DPADD=	${LIBZ}
LDADD=	-lz
CFLAGS=	-DMACHINE_ARCH=\"${MACHINE_ARCH}\"

.else

NOPROG=yes

.endif

MAN=	mkuboot.8

.include <bsd.prog.mk>
