#	$OpenBSD: Makefile,v 1.4 2006/04/29 16:26:56 maja Exp $

PROG=	mopchk
SRCS=	mopchk.c device.c version.c pf.c loop-bsd.c file.c
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/.. -I${.CURDIR}/../common
CLEANFILES= version.c version.h
DPADD=	${LIBKVM}
LDADD=	-lkvm
.PATH:	${.CURDIR}/.. ${.CURDIR}/../common 

version.c version.h: ${.CURDIR}/../common/VERSION
	rm -f version.c; \
	sed 's/.*/char version[] = "&";/' ${.ALLSRC} > version.c
	set `sed 's/\([0-9]*\)\.\([0-9]*\).*/\1 \2/' ${.ALLSRC}` ; \
		{ echo '#define VERSION_MAJOR' $$1 ; \
		  echo '#define VERSION_MINOR' $$2 ; } > version.h


.include <bsd.prog.mk>
