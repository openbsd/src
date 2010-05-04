# $FreeBSD: src/gnu/usr.bin/cc/include/Makefile,v 1.9.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.4 2010/05/04 18:37:00 naddy Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.ver"

.PATH: ${GCCDIR}/config/${GCC_CPU}

INCS?=
.if ${TARGET_ARCH} == "i386" || ${TARGET_ARCH} == "amd64"
INCS=	emmintrin.h mmintrin.h pmmintrin.h xmmintrin.h mm_malloc.h
.elif ${TARGET_ARCH} == "arm"
INCS=	mmintrin.h
.elif ${TARGET_ARCH} == "powerpc"
INCS=	ppc-asm.h altivec.h spe.h
.endif

mm_malloc.h: pmm_malloc.h
	@rm -rf ${.TARGET}
	@cp ${.ALLSRC} ${.TARGET}
CLEANFILES+=	mm_malloc.h

all:	${INCS}

clean: _SUBDIRUSE
	rm -f ${CLEANFILES}

depend:
	# Nothing here so far...

.if !empty(INCS)
install: ${INCS}
	${INSTALL} -C -o ${BINOWN} -g ${LOCALEGRP} -m ${NONBINMODE} ${.ALLSRC} \
		${DESTDIR}/usr/include
.else
install:
	# Nothing here so far...
.endif

.include <bsd.obj.mk>
