#	$OpenBSD: Makefile,v 1.2 1996/07/27 22:26:38 tholo Exp $

LIB=	z
HDRS=	zconf.h zlib.h
SRCS=	adler32.c compress.c crc32.c deflate.c gzio.c infblock.c \
	infcodes.c inffast.c inflate.c inftrees.c infutil.c trees.c \
	uncompr.c zutil.c
NOMAN=	noman

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
		j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		    ${DESTDIR}/usr/include"; \
		echo $$j; \
		eval "$$j"; \
	done

.include <bsd.lib.mk>
