#	$OpenBSD: Makefile.inc,v 1.11 2002/12/02 09:00:25 miod Exp $
#	$NetBSD: Makefile.inc,v 1.5 1996/02/01 22:33:01 mycroft Exp $

BINDIR=	/usr/mdec

RELOC_TEST=	0x340000
RELOC_SUN4=	0x240000
RELOC_SUN4C=	0x380000
RELOC_SUN4M=	0x440000

# Note: a `RELOC' value of 0x380000 seems to work on most machines
RELOC?=		${RELOC_TEST}
RELOC2!=	perl -e 'printf "0x%x", ${RELOC_TEST} + 0x40000'

CFLAGS+=	-fno-stack-protector

DEFS=		-DSTANDALONE -DRELOC=${RELOC} -DSUN4 -DSUN4C -DSUN_BOOTPARAMS

### Figure out what to use for libkern
LIBKERNDIR=		${.CURDIR}/../libkern

.if exists(${LIBKERNDIR}/obj.${MACHINE})
LIBKERNOBJDIR=		${LIBKERNDIR}/obj.${MACHINE}
.elif exists(${LIBKERNDIR}/obj)
LIBKERNOBJDIR=		${LIBKERNDIR}/obj
.else
LIBKERNOBJDIR=		${LIBKERNDIR}
.endif

LIBKERN=		${LIBKERNOBJDIR}/libkern.a

### Figure out what to use for libsa
LIBSADIR=		${.CURDIR}/../libsa

.if exists(${LIBSADIR}/obj.${MACHINE})
LIBSAOBJDIR=		${LIBSADIR}/obj.${MACHINE}
.elif exists(${LIBSADIR}/obj)
LIBSAOBJDIR=		${LIBSADIR}/obj
.else
LIBSAOBJDIR=		${LIBSADIR}
.endif

LIBSA=			${LIBSAOBJDIR}/libsa.a

### Figure out what to use for libz
LIBZDIR=		${.CURDIR}/../libz

.if exists(${LIBZDIR}/obj.${MACHINE})
LIBZOBJDIR=		${LIBZDIR}/obj.${MACHINE}
.elif exists(${LIBZDIR}/obj)
LIBZOBJDIR=		${LIBZDIR}/obj
.else
LIBZOBJDIR=		${LIBZDIR}
.endif

LIBZ=			${LIBZOBJDIR}/libz.a
