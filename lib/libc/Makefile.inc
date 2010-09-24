#	$OpenBSD: Makefile.inc,v 1.16 2010/09/24 13:33:00 matthew Exp $
#
# This file contains make rules used to build libc
#

.ifndef LIBCSRCDIR
all:
	@echo "need to define LIBCSRCDIR" >&2; exit 1
.endif

CFLAGS+=	-I${LIBCSRCDIR}/include

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
.include "${LIBCSRCDIR}/net/Makefile.inc"
.include "${LIBCSRCDIR}/nls/Makefile.inc"
.include "${LIBCSRCDIR}/ohash/Makefile.inc"
.if (${MACHINE_CPU} != "alpha")
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
.include "${LIBCSRCDIR}/sys/Makefile.inc"
.if (${YP:L} == "yes")
.include "${LIBCSRCDIR}/yp/Makefile.inc"
.endif

CFLAGS+=-DNLS

LIBKERN=	${LIBCSRCDIR}/../../sys/lib/libkern

KSRCS=	bcmp.c bzero.c ffs.c strcat.c strcmp.c strcpy.c strlen.c strncmp.c \
	strncpy.c htonl.c htons.c ntohl.c ntohs.c timingsafe_bcmp.c
.if (${MACHINE_CPU} != "alpha")
KSRCS+=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c iordi3.c \
	lshldi3.c lshrdi3.c moddi3.c muldi3.c negdi2.c notdi2.c qdivrem.c \
	subdi3.c  ucmpdi2.c udivdi3.c umoddi3.c xordi3.c
KINCLUDES+=	quad/quad.h
.endif

