#	$OpenBSD: Makefile,v 1.14 2013/01/05 11:20:56 miod Exp $

S=${.CURDIR}/../../../..

CFLAGS+=-I${.CURDIR}/../../include -I${.CURDIR}/../libbug -I${S}/lib/libsa
STAGE1_CFLAGS=-DSTAGE1
STAGE2_CFLAGS=-DSTAGE2
NETBOOT_CFLAGS=-DNETBOOT
SRCS=crt0.S
OBJS=stage1crt.o stage2crt.o netcrt.o
NOPIE=

CLEANFILES+=stage1crt.out stage2crt.out netcrt.out ${OBJS}

all:	${OBJS}

stage1crt.o: crt0.S
	${CC} ${CFLAGS} ${STAGE1_CFLAGS} -c ${.CURDIR}/crt0.S -o ${.TARGET}

stage2crt.o: crt0.S
	${CC} ${CFLAGS} ${STAGE2_CFLAGS} -c ${.CURDIR}/crt0.S -o ${.TARGET}

netcrt.o: crt0.S
	${CC} ${CFLAGS} ${NETBOOT_CFLAGS} -c ${.CURDIR}/crt0.S -o ${.TARGET}

install:

lint tags:

.include <bsd.prog.mk>
