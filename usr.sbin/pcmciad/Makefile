# $Id: Makefile,v 1.1 1996/04/29 13:08:51 hvozda Exp $

SRCS=	pcmciad.c pcmcia_conf.c
#LIBS=	-lutil
VPATH= ${.CURDIR}/../../sys/dev/pcmcia

CFLAGS+=	-g -Wmissing-prototypes -I${.CURDIR}/../../sys
PROG=	pcmciad
MAN=	pcmciad.8

pcmcia_conf.o: pcmcia_conf.c
	$(CC) $(CFLAGS) -D_KERNEL -c $<
clean::
	rm -f a.out [Ee]rrs mklog core *.core pcmciad pcmcia_conf.o pcmciad.o


SUBDIR=dumpcor dumpinfo dumpreg
.include <bsd.subdir.mk>
.include <bsd.prog.mk>
# DO NOT DELETE THIS LINE -- make depend depends on it.
