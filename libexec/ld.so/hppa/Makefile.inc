#	$OpenBSD: Makefile.inc,v 1.7 2022/12/05 07:49:07 kettenis Exp $

CFLAGS += -fpic
AFLAGS += -fpic
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

# extract components from libgcc.a
MDOBJ=	_dyncall.o _remU.o _remI.o _divI.o _divU.o _div_const.o
${MDOBJ}:
	ar x `$(CC) -print-libgcc-file-name` ${MDOBJ}
OBJS+=${MDOBJ}

RELATIVE_RELOC=R_PARISC_(IPLT|DIR32|PLABEL32)
