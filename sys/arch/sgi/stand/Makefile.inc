#	$OpenBSD: Makefile.inc,v 1.1 2004/08/23 14:22:40 pefo Exp $
#	$NetBSD: Makefile.inc,v 1.7 2000/08/20 14:57:16 mrg Exp $

.ifndef __INCLUDED_STAND_MAKEFILE_INC
__INCLUDED_STAND_MAKEFILE_INC=

#CEXTRAFLAGS?=-D_LP64

BINDIR=		/usr/mdec

STANDALONE?=	-D_STANDALONE

.if ${MACHINE} == "sgi"
CPPFLAGS+=	${STANDALONE}
CPPFLAGS+=	-I.
CPPFLAGS+=	-Derrno=errno

CFLAGS+=	-fno-stack-protector
CFLAGS+=	-fno-builtin-vprintf -fno-builtin-printf -fno-builtin-putchar
.endif



### Figure out what to use for libkern
LIBKERNDIR=             ${.CURDIR}/../libkern

.if exists(${LIBKERNDIR}/obj.${MACHINE})
LIBKERNOBJDIR=          ${LIBKERNDIR}/obj.${MACHINE}
.elif exists(${LIBKERNDIR}/obj)
LIBKERNOBJDIR=          ${LIBKERNDIR}/obj
.else 
LIBKERNOBJDIR=          ${LIBKERNDIR}
.endif

LIBKERN=                ${LIBKERNOBJDIR}/libkern.a

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
