#	$OpenBSD: Makefile,v 1.11 2003/12/17 01:15:25 millert Exp $

LIB=	z
HDRS=	zconf.h zlib.h
SRCS=	adler32.c compress.c crc32.c deflate.c gzio.c infback.c \
	inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c
MAN=	compress.3
MLINKS=	compress.3 zlibVersion.3 compress.3 deflateInit.3 \
	compress.3 deflate.3 compress.3 deflateEnd.3 \
	compress.3 inflateInit.3 compress.3 inflate.3 \
	compress.3 inflateEnd.3 compress.3 deflateInit2.3 \
	compress.3 deflateSetDictionary.3 \
	compress.3 deflateCopy.3 compress.3 deflateReset.3 \
	compress.3 deflateParams.3 compress.3 inflateInit2.3 \
	compress.3 inflateSetDictionary.3 compress.3 inflateSync.3 \
	compress.3 inflateReset.3 compress.3 compress2.3 \
	compress.3 uncompress.3 compress.3 gzopen.3 \
	compress.3 gzdopen.3 compress.3 gzsetparams.3 \
	compress.3 gzread.3 compress.3 gzwrite.3 \
	compress.3 gzprintf.3 compress.3 gzputs.3 \
	compress.3 gzgets.3 compress.3 gzputc.3 \
	compress.3 gzgetc.3 compress.3 gzflush.3 \
	compress.3 gzseek.3 compress.3 gzrewind.3 \
	compress.3 gztell.3 compress.3 gzeof.3 \
	compress.3 gzclose.3 compress.3 gzerror.3 \
	compress.3 adler32.3 compress.3 crc32.3 
CFLAGS+=-DHAVE_STRERROR -DHAVE_MEMCPY -DHAS_vsnprintf -DHAS_snprintf

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
		j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		    $$i ${DESTDIR}/usr/include"; \
		echo $$j; \
		eval "$$j"; \
	done

.include <bsd.lib.mk>
