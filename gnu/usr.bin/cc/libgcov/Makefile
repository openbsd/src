# $FreeBSD: src/gnu/lib/libgcov/Makefile,v 1.6 2009/06/21 01:54:47 kan Exp $
# $OpenBSD: Makefile,v 1.4 2019/12/21 21:40:00 espie Exp $

.include <bsd.own.mk>

GCCLIB= ${.CURDIR}/../../../gcc
GCCDIR= ${GCCLIB}/gcc

.include "../Makefile.tgt"

.if defined(CROSSDIR)
LD=${CROSSDIR}/usr/${GCC_TARGET}/bin/ld
RANLIB=${CROSSDIR}/usr/${GCC_TARGET}/bin/ranlib
.endif

.if defined(CROSS_TARGET)
#Building cc for target, CC should already be the cross compiler
.elif ${.OBJDIR} != ${.CURDIR}
CC=${.OBJDIR}/../cc/cc -B ${.OBJDIR}/../cc1
.else
CC=${.CURDIR}/../cc/cc -B ${.CURDIR}/../cc1
.endif

.include "${.CURDIR}/../Makefile.ver"

LIB=		gcov
LIBDIR=		/usr/lib/gcc-lib/${GCC_TARGET}/${BASEVER}
NOPROFILE=	Yes
NOPIC=		Yes

.include "${.CURDIR}/../Makefile.tgt"

.PATH: ${GCCDIR}/config/${GCC_CPU} ${GCCDIR}

CFLAGS+=	-DIN_GCC -DIN_LIBGCC2 -D__GCC_FLOAT_NOT_NEEDED
CFLAGS+=	-D_PTHREADS -DGTHREAD_USE_WEAK
CFLAGS+=	-I${.CURDIR}/../../usr.bin/cc/cc_tools \
		-I${GCCLIB}/include -I${GCCDIR}/config -I${GCCDIR} -I.

# Library members defined in libgcov.c.
# Defined in libgcov.c, included only in gcov library
SYMS = _gcov _gcov_merge_add _gcov_merge_single _gcov_merge_delta \
    _gcov_fork _gcov_execl _gcov_execlp _gcov_execle \
    _gcov_execv _gcov_execvp _gcov_execve \
    _gcov_interval_profiler _gcov_pow2_profiler _gcov_one_value_profiler

OBJS=		${SYMS:S/$/.o/}
OBJS_T=		${SYMS:S/$/.o/}
OBJS_P=		${SYMS:S/$/.po/}
OBJS_S=		${SYMS:S/$/.So/}

#-----------------------------------------------------------------------
#
# Helpful shortcuts for compiler invocations.
#
#HIDE =  -fvisibility=hidden -DHIDE_EXPORTS
CC_T =	${CC} -c ${CFLAGS} ${HIDE} -fPIC
CC_P =	${CC} -c ${CFLAGS} ${HIDE} -p -fPIC
CC_S =	${CC} -c ${CFLAGS} ${PICFLAG} -DSHARED

#-----------------------------------------------------------------------
#
# Generated headers
#
COMMONHDRS=	tm.h tconfig.h gcov-iov.h options.h

${COMMONHDRS}: ${.CURDIR}/../cc_tools/Makefile
	${MAKE} -f ${.ALLSRC} MFILE=${.ALLSRC} GCCDIR=${GCCDIR} ${.TARGET}

CLEANFILES += ${COMMONHDRS}
CLEANFILES += cs-*.h option*

BUILDFIRST = ${COMMONHDRS}
BUILDAFTER += ${OBJS_T} ${OBJS_P} ${OBJS_S}

${OBJS_T}: libgcov.c
	${CC_T} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}

.if !defined(NOPIC)
${OBJS_S}: libgcov.c
	${CC_S} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
.endif

.if !defined(NOPROFILE)
${OBJS_P}: libgcov.c
	${CC_P} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
.endif

.include <bsd.lib.mk>
