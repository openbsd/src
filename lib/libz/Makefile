#	$OpenBSD: Makefile,v 1.7 2003/02/26 18:13:22 millert Exp $

LIB=	z
HDRS=	zconf.h zlib.h
SRCS=	adler32.c compress.c crc32.c deflate.c gzio.c infblock.c \
	infcodes.c inffast.c inflate.c inftrees.c infutil.c trees.c \
	uncompr.c zutil.c
NOMAN=	noman
CFLAGS+=-DHAVE_UNISTD_H -DHAVE_STRERROR -DHAVE_MEMCPY -DHAS_vsnprintf \
	-DHAS_snprintf

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
		j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		    $$i ${DESTDIR}/usr/include"; \
		echo $$j; \
		eval "$$j"; \
	done

.include <bsd.lib.mk>
