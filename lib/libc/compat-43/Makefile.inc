#	$OpenBSD: Makefile.inc,v 1.5 2002/10/30 20:15:29 millert Exp $

# compat-43 sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/compat-43 ${LIBCSRCDIR}/compat-43

SRCS+=	creat.c getdtablesize.c gethostid.c getwd.c killpg.c sethostid.c \
	setpgrp.c setregid.c setreuid.c setrgid.c setruid.c sigcompat.c

MAN+=	creat.3 getdtablesize.3 gethostid.3 killpg.3 setregid.3 setreuid.3 \
	setruid.3 sigblock.3 sigpause.3 sigsetmask.3 sigvec.3

MLINKS+=setruid.3 setrgid.3
MLINKS+=gethostid.3 sethostid.3
MLINKS+=sigblock.3 sigmask.3
