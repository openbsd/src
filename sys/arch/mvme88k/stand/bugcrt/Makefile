#	$OpenBSD: Makefile,v 1.9 2003/10/01 20:39:41 miod Exp $
#
# DO NOT OPTIMIZE bugcrt (i.e. no "-O2")
# ...until it is fixed (-:
#
S=${.CURDIR}/../../../..

#CFLAGS+=-I${.CURDIR}/../../include -I${.CURDIR}/../libbug -I${S}/lib/libsa
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
