#	$OpenBSD: Makefile.inc,v 1.5 2023/01/11 17:16:33 deraadt Exp $

CFLAGS += -fpic -msoft-float -fno-jump-tables
AFLAGS += -fpic
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

.if ${COMPILER_VERSION} == "clang"
# extract components from libcompiler_rt.a
MDOBJ=	multi3.o
${MDOBJ}:
	ar x `$(CC) -print-libgcc-file-name` ${MDOBJ}
OBJS+=${MDOBJ}
.endif

RELATIVE_RELOC=R_SPARC_RELATIVE
