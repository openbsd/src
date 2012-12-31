#	$OpenBSD: Makefile,v 1.10 2012/12/31 21:35:32 miod Exp $
S=${.CURDIR}/../../../..
CFLAGS=-I${.CURDIR}/../../include -I${.CURDIR}/../libbug -I${S}/lib/libsa \
	-fomit-frame-pointer
AFLAGS+=-D_LOCORE
NOPIE=

OBJS=bugcrt.o bugcrtS.o

all:	${OBJS}

install:

lint tags:

.include <bsd.prog.mk>
