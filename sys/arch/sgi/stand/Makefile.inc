#	$OpenBSD: Makefile.inc,v 1.10 2012/10/19 13:51:59 miod Exp $
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
SAABI?=		-mips3 -mno-abicalls -D_NO_ABICALLS -G 0 -fno-pic -fno-common
AS?=		as
LD?=		ld
LIBSA_CPPFLAGS?= -DNEEDS_HEAP_H
.endif

### Figure out what to use for libsa
LIBSADIR?=               ${.CURDIR}/../libsa

.if exists(${LIBSADIR}/${__objdir})
LIBSAOBJDIR=            ${LIBSADIR}/${__objdir}
.else
LIBSAOBJDIR=            ${LIBSADIR}
.endif

LIBSA=                  ${LIBSAOBJDIR}/libsa.a

### Figure out what to use for libz
LIBZDIR?=                ${.CURDIR}/../libz

.if exists(${LIBZDIR}/${__objdir})
LIBZOBJDIR=             ${LIBZDIR}/${__objdir}
.else
LIBZOBJDIR=             ${LIBZDIR}
.endif

LIBZ=                   ${LIBZOBJDIR}/libz.a

.endif
