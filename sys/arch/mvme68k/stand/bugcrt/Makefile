#	$OpenBSD: Makefile,v 1.9 2012/08/21 14:46:19 pascal Exp $
#
# DO NOT OPTMIZE bugcrt (i.e. no "-O2")
#
S=${.CURDIR}/../../../..
CFLAGS=-I${.CURDIR}/../../include -I${.CURDIR}/../libbug -I${S}/lib/libsa \
	-fomit-frame-pointer
NOPIE=

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
