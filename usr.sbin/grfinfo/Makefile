#	$OpenBSD: Makefile,v 1.5 2002/09/06 22:08:36 miod Exp $

.if ${MACHINE} == "hp300"
PROG=	grfinfo
CFLAGS+= -I${.CURDIR}/../../sys/arch/hp300
DPADD=	${LIBUTIL}
LDADD=	-lutil
.else
NOPROG=yes
.endif

MAN=grfinfo.1
MANSUBDIR=hp300

.include <bsd.prog.mk>
