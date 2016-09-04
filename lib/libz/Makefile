#	$OpenBSD: Makefile,v 1.19 2016/09/04 09:54:25 natano Exp $

LIB=	z
HDRS=	zconf.h zlib.h
SRCS=	adler32.c compress.c crc32.c deflate.c gzio.c infback.c \
	inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c
MAN=	compress.3
CFLAGS+=-DHAVE_STRERROR -DHAVE_MEMCPY -DHAS_vsnprintf -DHAS_snprintf

PC_FILES=zlib.pc
CLEANFILES+=${PC_FILES}

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
		j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		    $$i ${DESTDIR}/usr/include"; \
		echo $$j; \
		eval "$$j"; \
	done

${PC_FILES}: zlib.h
	/bin/sh ${.CURDIR}/generate_pkgconfig.sh -c ${.CURDIR} -o ${.OBJDIR}

beforeinstall: ${PC_FILES}
	${INSTALL} ${INSTALL_COPY} -o root -g ${SHAREGRP} \
	    -m ${SHAREMODE} ${.OBJDIR}/${PC_FILES} ${DESTDIR}/usr/lib/pkgconfig/

.include <bsd.lib.mk>
