#	$OpenBSD: Makefile,v 1.3 1996/12/08 15:22:35 downsj Exp $

LIB=	z
HDRS=	zconf.h zlib.h
SRCS=	adler32.c compress.c crc32.c deflate.c gzio.c infblock.c \
	infcodes.c inffast.c inflate.c inftrees.c infutil.c trees.c \
	uncompr.c zutil.c
NOMAN=	noman

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
		j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		    ${DESTDIR}/usr/include"; \
		echo $$j; \
		eval "$$j"; \
	done

.include <bsd.lib.mk>
