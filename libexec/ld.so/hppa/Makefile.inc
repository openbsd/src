#	$OpenBSD: Makefile.inc,v 1.5 2019/10/21 02:36:43 guenther Exp $

CFLAGS += -fpic
AFLAGS += -fpic

# extract components from libgcc.a
MDOBJ=	_dyncall.o _remU.o _remI.o _divI.o _divU.o _div_const.o
${MDOBJ}:
	ar x `$(CC) -print-libgcc-file-name` ${MDOBJ}
OBJS+=${MDOBJ}
