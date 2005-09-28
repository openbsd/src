#	$OpenBSD: Makefile,v 1.13 2005/09/28 23:54:03 deraadt Exp $
#	$NetBSD: Makefile,v 1.13 1995/04/23 07:58:46 cgd Exp $

TARGET_MACHINE_ARCH?=	$(MACHINE_ARCH)

PROG=	ld
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c etc.c rrs.c xbits.c md.c
CPPFLAGS+=	-I$(.CURDIR) -I$(.CURDIR)/$(TARGET_MACHINE_ARCH)

.if (${MACHINE_ARCH} == "vax")
ld.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -O0 -c $<
.endif

.if ($(TARGET_MACHINE_ARCH) != $(MACHINE_ARCH))
CPPFLAGS+=	-DCROSS_LINKER -DXHOST=$(MACHINE_ARCH) \
	  	-Icross-include -U__ELF__
beforedepend:
	mkdir -p cross-include
	rm -f cross-include/${TARGET_MACHINE_ARCH}
	rm -f cross-include/${TARGET}
	rm -f cross-include/machine
	ln -sf ${.CURDIR}/../../../sys/arch/${TARGET_MACHINE_ARCH}/include \
		cross-include/${TARGET_MACHINE_ARCH}
	ln -sf ${.CURDIR}/../../../sys/arch/${TARGET}/include \
		cross-include/${TARGET}
	ln -sf ${.CURDIR}/../../../sys/arch/${TARGET}/include \
		cross-include/machine
.else
SUBDIR=	ldconfig ldd rtld
.endif

.PATH: $(.CURDIR)/$(TARGET_MACHINE_ARCH)

.include <bsd.prog.mk>
