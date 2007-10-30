#	$OpenBSD: Makefile,v 1.8 2007/10/30 06:03:14 deraadt Exp $
#
# DO NOT OPTMIZE bugcrt (i.e. no "-O2")
#
S=${.CURDIR}/../../../..
CFLAGS=-I${.CURDIR}/../../include -I${.CURDIR}/../libbug -I${S}/lib/libsa \
	-fomit-frame-pointer

OBJS=bugcrt.o

CLEANFILES+=bugcrt.out	

all:	${OBJS}

bugcrt.o: bugcrt.c
	${COMPILE.c} -c $<
	${LD} -x -r ${.TARGET} -o ${.TARGET}ut
	mv ${.TARGET}ut ${.TARGET}

install:

lint tags:

.include <bsd.prog.mk>
