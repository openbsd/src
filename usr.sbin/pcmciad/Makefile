# $Id: Makefile,v 1.2 1996/05/01 20:33:02 hvozda Exp $

SRCS=	pcmciad.c pcmcia_conf.c
VPATH=  ${.CURDIR}/../../sys/dev/pcmcia

CFLAGS+= -g -Wmissing-prototypes -I${.CURDIR}/../../sys
PROG=	pcmciad
MAN=	pcmciad.8

pcmcia_conf.o: pcmcia_conf.c
	$(CC) $(CFLAGS) -D_KERNEL -c $<
clean::
	rm -f a.out [Ee]rrs mklog core *.core pcmciad pcmcia_conf.o pcmciad.o


.include <bsd.prog.mk>
