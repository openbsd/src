#	$OpenBSD: Makefile,v 1.18 2014/07/05 05:31:35 guenther Exp $


PROG=	rdist
CFLAGS+=-I. -I${.CURDIR} -DOS_H=\"os-openbsd.h\"
SRCS=	gram.y child.c client.c common.c distopt.c docmd.c expand.c \
	isexec.c lookup.c message.c rdist.c
CLEANFILES+=gram.c y.tab.h
WARNINGS=yes

.include <bsd.prog.mk>

.depend: gram.c
