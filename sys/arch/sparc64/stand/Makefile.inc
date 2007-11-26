#	$OpenBSD: Makefile.inc,v 1.8 2007/11/26 10:26:02 deraadt Exp $
#	$NetBSD: Makefile.inc,v 1.7 2000/08/20 14:57:16 mrg Exp $

.ifndef __INCLUDED_STAND_MAKEFILE_INC
__INCLUDED_STAND_MAKEFILE_INC=

CEXTRAFLAGS?=-D_LP64
AFLAGS+= -Wa,-Av9a

BINDIR=		/usr/mdec

STANDALONE?=	-D_STANDALONE

.if ${MACHINE} == "sparc64"
CPPFLAGS+=	${STANDALONE} -DSUN4U
CPPFLAGS+=	-I.

CFLAGS+=	-fno-stack-protector
CFLAGS+=	-msoft-float
CFLAGS+=	-fno-builtin-vprintf -fno-builtin-printf -fno-builtin-putchar
.endif

### Figure out what to use for libsa
LIBSADIR=               ${.CURDIR}/../libsa

.if exists(${LIBSADIR}/obj.${MACHINE})
LIBSAOBJDIR=            ${LIBSADIR}/obj.${MACHINE}
.elif exists(${LIBSADIR}/obj)
LIBSAOBJDIR=            ${LIBSADIR}/obj
.else
LIBSAOBJDIR=            ${LIBSADIR}
.endif

LIBSA=                  ${LIBSAOBJDIR}/libsa.a

### Figure out what to use for libz
LIBZDIR=                ${.CURDIR}/../libz

.if exists(${LIBZDIR}/obj.${MACHINE})
LIBZOBJDIR=             ${LIBZDIR}/obj.${MACHINE}
.elif exists(${LIBZDIR}/obj)
LIBZOBJDIR=             ${LIBZDIR}/obj
.else
LIBZOBJDIR=             ${LIBZDIR}
.endif

LIBZ=                   ${LIBZOBJDIR}/libz.a

.endif
