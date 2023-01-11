#	$OpenBSD: Makefile.inc,v 1.10 2023/01/11 16:31:46 miod Exp $

CFLAGS += -fpic
CFLAGS+=-m4-nofpu
AFLAGS += -D_STANDALONE

# extract components from libgcc.a
MDOBJ=	_divdi3.o _clz.o _udiv_qrnnd_16.o _udivsi3_i4.o _udivdi3.o \
	_sdivsi3.o _udivsi3.o
${MDOBJ}:
	ar x `$(CC) -print-libgcc-file-name` ${MDOBJ}
OBJS+=${MDOBJ}

LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

RELATIVE_RELOC=R_SH_RELATIVE
