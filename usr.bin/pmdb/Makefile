#	$OpenBSD: Makefile,v 1.12 2010/02/03 20:49:01 miod Exp $

.include <bsd.own.mk>

MAN=pmdb.1

.if (!exists(arch/${MACHINE}/Makefile.inc) && \
    !exists(arch/${MACHINE_CPU}/Makefile.inc))
NOPROG=no, man
.else

PROG=pmdb
SRCS=pmdb.c symbol.c clit.c process.c signal.c break.c core.c
# support for NetBSD
.if defined(OBJECT_FMT)
.if ${OBJECT_FMT} == "ELF"
ELF_TOOLCHAIN=yes
.else
ELF_TOOLCHAIN=no
.endif
.endif

.if (${ELF_TOOLCHAIN:L} == "yes")
SRCS+=elf_syms.c
CPPFLAGS+=-DPMDB_ELF
.else
SRCS+=aout_syms.c
CPPFLAGS+=-DPMDB_AOUT
.endif

LDADD=-ledit -lcurses
DPADD=${LIBEDIT} ${LIBCURSES}

CPPFLAGS+=-I${.CURDIR}

.if exists(arch/${MACHINE}/Makefile.inc)
.PATH: ${.CURDIR}/arch/${MACHINE}
.include "arch/${MACHINE}/Makefile.inc"
CPPFLAGS+=-I${.CURDIR}/arch/${MACHINE}
.endif

.if (${MACHINE_CPU} != ${MACHINE}) && exists(arch/${MACHINE_CPU}/Makefile.inc)
.PATH: ${.CURDIR}/arch/${MACHINE_CPU}
.include "arch/${MACHINE_CPU}/Makefile.inc"
CPPFLAGS+=-I${.CURDIR}/arch/${MACHINE_CPU}
.endif

COPTS+=-Wall -Wno-uninitialized

.endif

.include <bsd.prog.mk>
