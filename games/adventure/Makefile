#	$NetBSD: Makefile,v 1.3 1995/03/21 12:04:53 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/12/93

PROG=	adventure
SRCS=	main.c init.c done.c save.c subr.c vocab.c wizard.c io.c data.c crc.c
MAN=	adventure.6
CFLAGS+=-traditional-cpp
HIDEGAME=hidegame
CLEANFILES+=setup data.c

data.c: glorkz setup
	./setup ${.CURDIR}/glorkz > data.c

setup: setup.c hdr.h
	${CC} -o setup ${.CURDIR}/setup.c

.include <bsd.prog.mk>
