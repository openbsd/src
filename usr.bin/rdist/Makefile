#	$OpenBSD: Makefile,v 1.20 2017/07/09 14:04:50 espie Exp $


PROG=	rdist
CFLAGS+=-I. -I${.CURDIR}
SRCS=	gram.y child.c client.c common.c distopt.c docmd.c expand.c \
	isexec.c lookup.c message.c rdist.c
WARNINGS=yes

.include <bsd.prog.mk>

.depend: gram.c
