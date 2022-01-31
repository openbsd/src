#	$OpenBSD: Makefile.inc,v 1.6 2022/01/31 05:43:22 guenther Exp $

CFLAGS += -fpic
AFLAGS += -fpic

# extract components from libgcc.a
MDOBJ=	_dyncall.o _remU.o _remI.o _divI.o _divU.o _div_const.o
${MDOBJ}:
	ar x `$(CC) -print-libgcc-file-name` ${MDOBJ}
OBJS+=${MDOBJ}

RELATIVE_RELOC=R_PARISC_(IPLT|DIR32|PLABEL32)
