# $FreeBSD: src/gnu/usr.bin/cc/include/Makefile,v 1.9.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.14 2019/12/21 21:40:00 espie Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.ver"

.PATH: ${GCCDIR}/config/${GCC_CPU}

INCS?=
.if ${TARGET_ARCH} == "i386" || ${TARGET_ARCH} == "amd64"
INCS=	cpuid.h emmintrin.h mmintrin.h pmmintrin.h xmmintrin.h mm_malloc.h
.elif ${TARGET_ARCH} == "arm"
INCS=	mmintrin.h
.elif ${TARGET_ARCH} == "powerpc"
INCS=	ppc-asm.h altivec.h spe.h
.endif
INCS += unwind.h

mm_malloc.h: pmm_malloc.h
	@rm -rf ${.TARGET}
	@cp ${.ALLSRC} ${.TARGET}
CLEANFILES+=	mm_malloc.h

.if ${TARGET_ARCH} == "arm"
unwind.h: ${GCCDIR}/config/arm/unwind-arm.h
	@rm -rf ${.TARGET}
	@cp ${.ALLSRC} ${.TARGET}
.else
unwind.h: ${GCCDIR}/unwind-generic.h
	@rm -rf ${.TARGET}
	@cp ${.ALLSRC} ${.TARGET}
.endif
CLEANFILES+=	unwind.h

all:	${INCS}

clean: _SUBDIRUSE
	rm -f ${CLEANFILES}

cleandir: _SUBDIRUSE clean

.if ${TARGET_ARCH} == "i386" || ${TARGET_ARCH} == "amd64"
prereq: mm_malloc.h unwind.h
.else
prereq: unwind.h
.endif

.if !empty(INCS)
install includes: ${INCS}
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m ${NONBINMODE} ${.ALLSRC} \
		${DESTDIR}/usr/include
.else
install includes:
	# Nothing here so far...
.endif

tags:
	# Nothing here so far...

.include <bsd.obj.mk>
