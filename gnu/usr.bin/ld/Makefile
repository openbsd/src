#	$OpenBSD: Makefile,v 1.4 1999/02/09 04:54:29 smurph Exp $
#	$NetBSD: Makefile,v 1.13 1995/04/23 07:58:46 cgd Exp $

TARGET_MACHINE_ARCH?=	$(MACHINE_ARCH)

PROG=	ld
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c etc.c rrs.c xbits.c md.c
CFLAGS+=-g 
CPPFLAGS+=	-I$(.CURDIR) -I$(.CURDIR)/$(TARGET_MACHINE_ARCH)
.if ($(MACHINE_ARCH) == "m88k")
MACHINE_ARCH = mvme88k
.endif
.if ($(TARGET_MACHINE_ARCH) != $(MACHINE_ARCH))
CPPFLAGS+=	-DCROSS_LINKER -DXHOST=$(MACHINE_ARCH) \
		-I${.CURDIR}/../../../sys/arch/${TARGET_MACHINE_ARCH}/include \
		-I${.CURDIR}/../../../include -I${.CURDIR}/../../..
.else
SUBDIR=	ldconfig ldd
.endif

.PATH: $(.CURDIR)/$(TARGET_MACHINE_ARCH)

.include <bsd.prog.mk>
.if !defined(NOPIC) && ($(TARGET_MACHINE_ARCH) == $(MACHINE_ARCH))
SUBDIR+= rtld
.endif
