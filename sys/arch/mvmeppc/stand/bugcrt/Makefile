#	$OpenBSD: Makefile,v 1.2 2004/11/15 14:03:06 miod Exp $
#
# DO NOT OPTMIZE bugcrt (i.e. no "-O2")
#
S=${.CURDIR}/../../../..

CFLAGS=-I${.CURDIR}/../../include -I${.CURDIR}/../libbug -I${S}/lib/libsa \
	-fomit-frame-pointer
STAGE1_CFLAGS=-DSTACK_ASM_OP="\".long ${STAGE1_RELOC}\"" -DSTAGE1
STAGE2_CFLAGS=-DSTACK_ASM_OP="\".long ${STAGE2_RELOC}\""
SINGLE_CFLAGS=-DSTACK_ASM_OP="\".long ${STAGE2_RELOC}\"" -DSTAGE1
SRCS=crt.c
OBJS=stage1crt.o stage2crt.o singlecrt.o

CLEANFILES+=a.out ${OBJS}

all:	${OBJS}

stage1crt.o: crt.c
	${CC} ${CFLAGS} ${STAGE1_CFLAGS} -c ${.CURDIR}/crt.c -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

stage2crt.o: crt.c
	${CC} ${CFLAGS} ${STAGE2_CFLAGS} -c ${.CURDIR}/crt.c -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

singlecrt.o: crt.c
	${CC} ${CFLAGS} ${SINGLE_CFLAGS} -c ${.CURDIR}/crt.c -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

install:

lint tags:

.include <bsd.prog.mk>
