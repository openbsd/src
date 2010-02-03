#	$OpenBSD: Makefile.inc,v 1.3 2010/02/03 20:49:00 miod Exp $

# gdtoa sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_CPU}/gdtoa ${LIBCSRCDIR}/gdtoa

CFLAGS+= -I${LIBCSRCDIR}/gdtoa
CFLAGS+= -I${LIBCSRCDIR}/arch/${MACHINE_CPU}/gdtoa

CFLAGS+= -DINFNAN_CHECK
CFLAGS+= -DMULTIPLE_THREADS
CFLAGS+= -DNO_FENV_H
CFLAGS+= -DUSE_LOCALE

SRCS+= dmisc.c dtoa.c gdtoa.c gethex.c gmisc.c hd_init.c hdtoa.c \
	hexnan.c ldtoa.c locks.c misc.c smisc.c strtod.c strtodg.c \
	strtof.c strtold.c sum.c ulp.c

.include "${LIBCSRCDIR}/arch/${MACHINE_CPU}/gdtoa/Makefile.inc"
