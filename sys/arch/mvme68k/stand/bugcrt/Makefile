#	$OpenBSD: Makefile,v 1.7 1997/04/22 16:05:38 gvf Exp $
#
# DO NOT OPTMIZE bugcrt (i.e. no "-O2")
#
S=${.CURDIR}/../../../..
CFLAGS=-I${.CURDIR}/../../include -I${.CURDIR}/../libbug -I${S}/lib/libsa \
	-fomit-frame-pointer

OBJS=bugcrt.o

CLEANFILES+=a.out

all:	${OBJS}

bugcrt.o: bugcrt.c
	${COMPILE.c} -c $<
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

install:

lint tags:

.include <bsd.prog.mk>
