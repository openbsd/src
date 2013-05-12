#	$OpenBSD: Makefile,v 1.15 2013/05/12 10:43:45 miod Exp $

S=${.CURDIR}/../../../..

CFLAGS+=-I${.CURDIR}/../../include -I${.CURDIR}/../libbug -I${S}/lib/libsa
STAGE1_CFLAGS=	-DLOAD_FROM_VID
STAGE2_CFLAGS=	-DLOAD_FROM_BOOTXX
NETBOOT_CFLAGS=	-DNETBOOT
SREC_CFLAGS=	-DSREC
SRCS=crt0.S
OBJS=stage1crt.o stage2crt.o netcrt.o sreccrt.o
NOPIE=

all:	${OBJS}

stage1crt.o: crt0.S
	${CC} ${CFLAGS} ${STAGE1_CFLAGS} -c ${.CURDIR}/crt0.S -o ${.TARGET}

stage2crt.o: crt0.S
	${CC} ${CFLAGS} ${STAGE2_CFLAGS} -c ${.CURDIR}/crt0.S -o ${.TARGET}

netcrt.o: crt0.S
	${CC} ${CFLAGS} ${NETBOOT_CFLAGS} -c ${.CURDIR}/crt0.S -o ${.TARGET}

sreccrt.o: crt0.S
	${CC} ${CFLAGS} ${SREC_CFLAGS} -c ${.CURDIR}/crt0.S -o ${.TARGET}

install:

lint tags:

.include <bsd.prog.mk>
