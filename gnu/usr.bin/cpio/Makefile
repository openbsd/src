#	$OpenBSD: Makefile,v 1.4 1997/04/27 20:55:55 millert Exp $
#	$NetBSD: Makefile,v 1.3 1995/04/23 07:58:27 cgd Exp $

PROG= cpio
CFLAGS+= -I${.CURDIR} -DRETSIGTYPE=void -DHAVE_SYS_MTIO_H=1 -DSTDC_HEADERS=1 -DHAVE_UNISTD_H=1 -DHAVE_STRING_H=1 -DHAVE_FCNTL_H=1 -DHAVE_UTIME_H=1 -DHAVE_STRERROR=1 -DHAVE_LCHOWN=1 -DHAVE_VPRINTF=1 -DHAVE_ALLOCA=1 -DHAVE_DIRENT_H=1 -DHAVE_SYS_CDEFS_H=1

MAKEINFO=	makeinfo

SRCS=	copyin.c copyout.c copypass.c defer.c dstring.c fnmatch.c global.c \
	main.c tar.c util.c error.c filemode.c getopt.c getopt1.c version.c \
	rtapelib.c dirname.c idcache.c makepath.c xmalloc.c stripslash.c \
	userspec.c xstrdup.c
CLEANFILES+=	cpio.info

all:	cpio.info

cpio.info:	cpio.texi
	${MAKEINFO} -I${.CURDIR} ${.CURDIR}/cpio.texi

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -m 444 -o ${BINOWN} -g ${BINGRP} *.info* \
		${DESTDIR}/usr/share/info

.include <bsd.prog.mk>
