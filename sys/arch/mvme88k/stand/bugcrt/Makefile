#	$OpenBSD: Makefile,v 1.5 1998/12/15 06:12:50 smurph Exp $
#
# DO NOT OPTMIZE bugcrt (i.e. no "-O2")
#
S=${.CURDIR}/../../../..
CFLAGS=-I${.CURDIR}/../../include -I${.CURDIR}/../libbug -I${S}/lib/libsa \
	-fomit-frame-pointer

OBJS=bugcrt.o sdcrt.o

CLEANFILES+=a.out ${OBJS}

all:	${OBJS}

bugcrt.o: bugcrt.c
	${COMPILE.c} -c $<
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

sdcrt.o: sdcrt.c
	${COMPILE.c} -c $<
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

install:

lint tags:

.include <bsd.prog.mk>
