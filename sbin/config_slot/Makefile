#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:36:11 deraadt Exp $

SRCS=	config_slot.c pcmcia_conf.c
#dumpreg.c

.PATH:	${.CURDIR}/../../sys/dev/pcmcia

CFLAGS+=-Wmissing-prototypes -I${.CURDIR}/../../sys
PROG=	config_slot
NOMAN=

pcmcia_conf.o: pcmcia_conf.c
	${COMPILE.c} -D_KERNEL $<

.include <bsd.prog.mk>
