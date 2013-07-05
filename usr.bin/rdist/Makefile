#	$OpenBSD: Makefile,v 1.17 2013/07/05 21:29:51 miod Exp $


PROG=	rdist
CFLAGS+=-I. -I${.CURDIR} -DOS_H=\"os-openbsd.h\"
SRCS=	gram.y child.c client.c common.c distopt.c docmd.c expand.c \
	isexec.c lookup.c message.c rdist.c
CLEANFILES+=gram.c y.tab.h
WARNINGS=yes

.include <bsd.prog.mk>

CFLAGS+=-DEXE_TYPE=EXE_ELF

.depend: gram.c
