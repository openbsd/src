#	$OpenBSD: Makefile,v 1.7 2000/02/01 21:01:38 espie Exp $
#	$NetBSD: Makefile,v 1.13 1995/04/23 07:58:46 cgd Exp $

TARGET_MACHINE_ARCH?=	$(MACHINE_ARCH)

PROG=	ld
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c etc.c rrs.c xbits.c md.c
CPPFLAGS+=	-I$(.CURDIR) -I$(.CURDIR)/$(TARGET_MACHINE_ARCH)

.if ($(TARGET_MACHINE_ARCH) != $(MACHINE_ARCH))
CPPFLAGS+=	-DCROSS_LINKER -DXHOST=$(MACHINE_ARCH) \
	  	-Icross-include
beforedepend:
	mkdir -p cross-include
	-rm cross-include/${TARGET_MACHINE_ARCH}
	ln -sf ${.CURDIR}/../../../sys/arch/${TARGET_MACHINE_ARCH}/include \
		cross-include/${TARGET_MACHINE_ARCH}
.else
SUBDIR=	ldconfig ldd
.endif

.PATH: $(.CURDIR)/$(TARGET_MACHINE_ARCH)

.include <bsd.prog.mk>
.if !defined(NOPIC) && ($(TARGET_MACHINE_ARCH) == $(MACHINE_ARCH))
SUBDIR+= rtld
.endif
