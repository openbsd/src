#	$OpenBSD: Makefile,v 1.1 1996/07/27 02:39:42 tholo Exp $

LIB=	z
HDRS=	zconf.h zlib.h
SRCS=	adler32.c compress.c crc32.c deflate.c gzio.c infblock.c \
	infcodes.c inffast.c inflate.c inftrees.c infutil.c trees.c \
	uncompr.c zutil.c
NOMAN=	noman

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
		j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    install -c -o ${BINOWN} -g ${BINGRP} 0m 444 $$i \
		    ${DESTDIR}/usr/include"; \
		echo $$jl \
		eval "$$j"; \
	done

.include <bsd.lib.mk>
