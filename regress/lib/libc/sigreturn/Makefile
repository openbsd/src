# $OpenBSD: Makefile,v 1.5 2002/09/02 20:01:43 avsm Exp $

PROG=	sigret

DEBUG+= -ggdb

REGRESS_TARGETS+= sigret-normal sigret-indirect sigret-altstack

sigret-normal: ${PROG}
	./${PROG}

sigret-indirect: ${PROG}
	./${PROG} -i

sigret-altstack: ${PROG}
	./${PROG} -a

.include <bsd.regress.mk>
