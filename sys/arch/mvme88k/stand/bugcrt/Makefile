#	$OpenBSD: Makefile,v 1.10 2003/10/02 13:24:39 miod Exp $

S=${.CURDIR}/../../../..

CFLAGS+=-I${.CURDIR}/../../include -I${.CURDIR}/../libbug -I${S}/lib/libsa
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
