#	$OpenBSD: Makefile,v 1.3 2002/05/11 00:20:20 espie Exp $

PROG=	mopd
SRCS=	mopd.c process.c print.c cmp.c get.c put.c mopdef.c nma.c device.c \
	version.c pf.c loop-bsd.c dl.c rc.c file.c
MAN=	mopd.8
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/.. -I${.CURDIR}/../common
CLEANFILES= version.c version.h
DPADD=	${LIBKVM}
LDADD=	-lkvm
.PATH:	${.CURDIR}/../common

version.c version.h: ${.CURDIR}/../common/VERSION
	rm -f version.c; \
	sed 's/.*/char version[] = "&";/' ${.ALLSRC} > version.c
	set `sed 's/\([0-9]*\)\.\([0-9]*\).*/\1 \2/' ${.ALLSRC}` ; \
		{ echo '#define VERSION_MAJOR' $$1 ; \
		  echo '#define VERSION_MINOR' $$2 ; } > version.h


.include <bsd.prog.mk>
