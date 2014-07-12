#	$OpenBSD: Makefile,v 1.19 2014/07/12 03:32:00 guenther Exp $


PROG=	rdist
CFLAGS+=-I. -I${.CURDIR}
SRCS=	gram.y child.c client.c common.c distopt.c docmd.c expand.c \
	isexec.c lookup.c message.c rdist.c
CLEANFILES+=gram.c y.tab.h
WARNINGS=yes

.include <bsd.prog.mk>

.depend: gram.c
