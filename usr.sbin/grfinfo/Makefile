#	$OpenBSD: Makefile,v 1.4 2000/05/18 16:46:38 espie Exp $

.if ${MACHINE} == "hp300"
PROG=	grfinfo
CFLAGS+= -I${.CURDIR}/../../sys/arch/hp300
.else
NOPROG=yes
.endif

MAN=grfinfo.1
MANSUBDIR=hp300

.include <bsd.prog.mk>
