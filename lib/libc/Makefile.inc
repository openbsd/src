#	$OpenBSD: Makefile.inc,v 1.27 2015/11/25 05:01:44 guenther Exp $
#
# This file contains make rules used to build libc
#

.ifndef LIBCSRCDIR
all:
	@echo "need to define LIBCSRCDIR" >&2; exit 1
.endif

CFLAGS+=-I${LIBCSRCDIR}/include -I${LIBCSRCDIR}/hidden \
	-Werror-implicit-function-declaration -include namespace.h

.if ${COMPILER_VERSION:L} != "gcc3"
CFLAGS+=-Werror=deprecated-declarations
.endif

# Include link-time warnings about unsafe API uses (ie. strcpy)
CFLAGS+=-DAPIWARN

.if (${YP:L} == "yes")
CFLAGS+=-DYP -I${LIBCSRCDIR}/yp
.endif

LLIBS=
AINC=   -I${LIBCSRCDIR}/arch/${MACHINE_CPU}
.if defined(DESTDIR)
AINC+=  -nostdinc -idirafter ${DESTDIR}/usr/include
.endif

.if exists (${LIBCSRCDIR}/arch/${MACHINE_CPU}/Makefile.inc)
.PATH:	${LIBCSRCDIR}/arch/${MACHINE_CPU}
.include "${LIBCSRCDIR}/arch/${MACHINE_CPU}/Makefile.inc"
.endif

.include "${LIBCSRCDIR}/db/Makefile.inc"
.include "${LIBCSRCDIR}/dlfcn/Makefile.inc"
.include "${LIBCSRCDIR}/citrus/Makefile.inc"
.include "${LIBCSRCDIR}/compat-43/Makefile.inc"
.include "${LIBCSRCDIR}/gen/Makefile.inc"
.include "${LIBCSRCDIR}/crypt/Makefile.inc"
.include "${LIBCSRCDIR}/gdtoa/Makefile.inc"
.include "${LIBCSRCDIR}/gmon/Makefile.inc"
.include "${LIBCSRCDIR}/hash/Makefile.inc"
.include "${LIBCSRCDIR}/locale/Makefile.inc"
.include "${LIBCSRCDIR}/asr/Makefile.inc"
.include "${LIBCSRCDIR}/net/Makefile.inc"
.include "${LIBCSRCDIR}/nls/Makefile.inc"

# 32-bit systems need these
.if (${MACHINE_CPU} == "i386") || (${MACHINE_CPU} == "powerpc") || \
    (${MACHINE_CPU} == "hppa") || (${MACHINE_CPU} == "arm") || \
    (${MACHINE_CPU} == "sparc") || (${MACHINE_CPU} == "vax") || \
    (${MACHINE_CPU} == "sh") || (${MACHINE_CPU} == "m88k")
.include "${LIBCSRCDIR}/quad/Makefile.inc"
.endif

.include "${LIBCSRCDIR}/regex/Makefile.inc"
.include "${LIBCSRCDIR}/rpc/Makefile.inc"
.include "${LIBCSRCDIR}/stdio/Makefile.inc"
.include "${LIBCSRCDIR}/stdlib/Makefile.inc"
.include "${LIBCSRCDIR}/string/Makefile.inc"
.include "${LIBCSRCDIR}/termios/Makefile.inc"
.include "${LIBCSRCDIR}/thread/Makefile.inc"
.include "${LIBCSRCDIR}/time/Makefile.inc"
.include "${LIBCSRCDIR}/uuid/Makefile.inc"
.include "${LIBCSRCDIR}/sys/Makefile.inc"
.if (${YP:L} == "yes")
.include "${LIBCSRCDIR}/yp/Makefile.inc"
.endif

${OBJS} ${GOBJS} ${POBJS} ${SOBJS} ${DOBJS}: ${LIBCSRCDIR}/include/namespace.h
