#	$OpenBSD: Makefile,v 1.2 1996/09/21 19:12:37 maja Exp $

PROG=	moptrace
SRCS=	moptrace.c print.c cmp.c get.c mopdef.c nma.c device.c version.c pf.c \
	loop-bsd.c dl.c rc.c
MAN=	moptrace.1
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/.. -I${.CURDIR}/../common
CLEANFILES= version.c version.h
LDADD=	-lkvm
.PATH:	${.CURDIR}/../common

version.c version.h: ${.CURDIR}/../common/VERSION
	rm -f version.c; \
	sed 's/.*/char version[] = "&";/' ${.ALLSRC} > version.c
	set `sed 's/\([0-9]*\)\.\([0-9]*\).*/\1 \2/' ${.ALLSRC}` ; \
		{ echo '#define VERSION_MAJOR' $$1 ; \
		  echo '#define VERSION_MINOR' $$2 ; } > version.h


.include <bsd.prog.mk>
