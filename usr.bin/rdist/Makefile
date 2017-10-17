#	$OpenBSD: Makefile,v 1.21 2017/10/17 19:31:56 naddy Exp $


PROG=	rdist
CFLAGS+=-I. -I${.CURDIR}
SRCS=	gram.y child.c client.c common.c distopt.c docmd.c expand.c \
	isexec.c lookup.c message.c rdist.c
WARNINGS=yes

.include <bsd.prog.mk>
