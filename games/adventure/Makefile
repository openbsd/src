#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:35:13 deraadt Exp $

PROG=	adventure
SRCS=	main.c init.c done.c save.c subr.c vocab.c wizard.c io.c data.c crc.c
MAN=	adventure.6
CFLAGS+=-traditional-cpp
HIDEGAME=hidegame
CLEANFILES+=setup data.c

data.c: glorkz setup
	./setup ${.CURDIR}/glorkz > data.c

setup: setup.c hdr.h
	${HOSTCC} -o setup ${.CURDIR}/setup.c

.include <bsd.prog.mk>
