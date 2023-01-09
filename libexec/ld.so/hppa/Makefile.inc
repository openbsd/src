#	$OpenBSD: Makefile.inc,v 1.8 2023/01/09 13:52:42 kettenis Exp $

CFLAGS += -fpic -fno-jump-tables
AFLAGS += -fpic
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

# extract components from libgcc.a
MDOBJ=	_dyncall.o _remU.o _remI.o _divI.o _divU.o _div_const.o
${MDOBJ}:
	ar x `$(CC) -print-libgcc-file-name` ${MDOBJ}
OBJS+=${MDOBJ}

RELATIVE_RELOC=R_PARISC_(IPLT|DIR32|PLABEL32)
