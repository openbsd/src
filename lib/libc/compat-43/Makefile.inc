#	$OpenBSD: Makefile.inc,v 1.8 2010/02/03 20:49:00 miod Exp $

# compat-43 sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_CPU}/compat-43 ${LIBCSRCDIR}/compat-43

SRCS+=	creat.c getdtablesize.c gethostid.c getwd.c killpg.c sethostid.c \
	setpgrp.c sigcompat.c

MAN+=	creat.3 getdtablesize.3 gethostid.3 killpg.3 \
	sigblock.3 sigpause.3 sigsetmask.3 sigvec.3

MLINKS+=gethostid.3 sethostid.3
MLINKS+=sigblock.3 sigmask.3
