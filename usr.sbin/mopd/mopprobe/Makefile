#	$OpenBSD: Makefile,v 1.4 2002/05/11 00:20:20 espie Exp $

PROG=	mopprobe
SRCS=	mopprobe.c device.c get.c cmp.c mopdef.c version.c pf.c loop-bsd.c \
	print.c nma.c
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/.. -I${.CURDIR}/../common -DNODL
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
