# $OpenBSD: Makefile,v 1.3 1996/08/21 06:04:08 tholo Exp $

SRCS=	config_slot.c pcmcia_conf.c
#dumpreg.c

.PATH:	${.CURDIR}/../../sys/dev/pcmcia

CFLAGS+=-Wmissing-prototypes -I${.CURDIR}/../../sys
PROG=	config_slot
NOMAN=

pcmcia_conf.o: pcmcia_conf.c
	$(CC) $(CFLAGS) -D_KERNEL -c $<

.include <bsd.prog.mk>
