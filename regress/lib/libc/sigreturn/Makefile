# $OpenBSD: Makefile,v 1.3 2002/01/03 19:17:24 marc Exp $

PROG=	sigret

DEBUG+= -ggdb

REGRESSTARGETS+= sigret-normal sigret-indirect sigret-altstack

sigret-normal:
	./${PROG}

sigret-indirect: ${PROG}
	./${PROG} -i

sigret-altstack: ${PROG}
	./${PROG} -a

.include <bsd.regress.mk>
