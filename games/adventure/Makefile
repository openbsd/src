#	$OpenBSD: Makefile,v 1.4 2001/02/04 06:07:20 pjanzen Exp $

PROG=	adventure
SRCS=	main.c init.c done.c save.c subr.c vocab.c wizard.c io.c data.c crc.c
MAN=	adventure.6
HIDEGAME=hidegame
CLEANFILES+=setup data.c

data.c: glorkz setup
	./setup ${.CURDIR}/glorkz > data.c

setup: setup.c hdr.h
	${HOSTCC} -o setup ${.CURDIR}/setup.c

.include <bsd.prog.mk>
