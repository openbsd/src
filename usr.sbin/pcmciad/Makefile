#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:44:02 deraadt Exp $

SRCS=	pcmciad.c pcmcia_conf.c
.PATH:  ${.CURDIR}/../../sys/dev/pcmcia

CFLAGS+=-Wmissing-prototypes -I${.CURDIR}/../../sys
PROG=	pcmciad
MAN=	pcmciad.8

pcmcia_conf.o: pcmcia_conf.c
	${COMPILE.c} -D_KERNEL $<
clean::
	rm -f a.out [Ee]rrs mklog core *.core pcmciad pcmcia_conf.o pcmciad.o


.include <bsd.prog.mk>
