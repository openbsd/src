#	$OpenBSD: Makefile,v 1.3 2007/10/30 06:03:16 deraadt Exp $
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
	${LD} -x -r ${.TARGET} -o ${.TARGET}ut 
	mv ${.TARGET}ut ${.TARGET}

stage2crt.o: crt.c
	${CC} ${CFLAGS} ${STAGE2_CFLAGS} -c ${.CURDIR}/crt.c -o ${.TARGET}
	${LD} -x -r ${.TARGET} -o ${.TARGET}ut 
	mv ${.TARGET}ut ${.TARGET}

singlecrt.o: crt.c
	${CC} ${CFLAGS} ${SINGLE_CFLAGS} -c ${.CURDIR}/crt.c -o ${.TARGET}
	${LD} -x -r ${.TARGET} -o ${.TARGET}ut 
	mv ${.TARGET}ut ${.TARGET}

install:

lint tags:

.include <bsd.prog.mk>
