# $OpenBSD: Makefile,v 1.4 1997/03/20 19:10:20 niklas Exp $

SRCS=	config_slot.c pcmcia_conf.c
#dumpreg.c

.PATH:	${.CURDIR}/../../sys/dev/pcmcia

CFLAGS+=-Wmissing-prototypes -I${.CURDIR}/../../sys
PROG=	config_slot
NOMAN=

pcmcia_conf.o: pcmcia_conf.c
	${COMPILE.c} -D_KERNEL $<

.include <bsd.prog.mk>
