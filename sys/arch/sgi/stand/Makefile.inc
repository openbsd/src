#	$OpenBSD: Makefile.inc,v 1.6 2010/09/14 14:28:05 marco Exp $
#	$NetBSD: Makefile.inc,v 1.7 2000/08/20 14:57:16 mrg Exp $

.ifndef __INCLUDED_STAND_MAKEFILE_INC
__INCLUDED_STAND_MAKEFILE_INC=

BINDIR=		/usr/mdec

STANDALONE?=	-D_STANDALONE

.if ${MACHINE} == "sgi"
CPPFLAGS+=	${STANDALONE}
CPPFLAGS+=	-I.

CFLAGS+=	-fno-stack-protector -Wall
CFLAGS+=	-fno-builtin-vprintf -fno-builtin-printf -fno-builtin-putchar
CFLAGS+=	-fno-builtin-exit
SAABI?=		-mips3 -mno-abicalls -G 0 -fno-pic -fno-common
AS?=		as
LD?=		ld
LIBSA_CPPFLAGS?= -DNEEDS_HEAP_H
.endif

### Figure out what to use for libsa
LIBSADIR?=               ${.CURDIR}/../libsa

.if exists(${LIBSADIR}/obj.${MACHINE})
LIBSAOBJDIR=            ${LIBSADIR}/obj.${MACHINE}
.elif exists(${LIBSADIR}/obj)
LIBSAOBJDIR=            ${LIBSADIR}/obj
.else
LIBSAOBJDIR=            ${LIBSADIR}
.endif

LIBSA=                  ${LIBSAOBJDIR}/libsa.a

### Figure out what to use for libz
LIBZDIR?=                ${.CURDIR}/../libz

.if exists(${LIBZDIR}/obj.${MACHINE})
LIBZOBJDIR=             ${LIBZDIR}/obj.${MACHINE}
.elif exists(${LIBZDIR}/obj)
LIBZOBJDIR=             ${LIBZDIR}/obj
.else
LIBZOBJDIR=             ${LIBZDIR}
.endif

LIBZ=                   ${LIBZOBJDIR}/libz.a

.endif
