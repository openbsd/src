# $OpenBSD: Makefile,v 1.4 2002/01/04 13:53:32 art Exp $

PROG=	sigret

DEBUG+= -ggdb

REGRESSTARGETS+= sigret-normal sigret-indirect sigret-altstack

sigret-normal: ${PROG}
	./${PROG}

sigret-indirect: ${PROG}
	./${PROG} -i

sigret-altstack: ${PROG}
	./${PROG} -a

.include <bsd.regress.mk>
