#	$OpenBSD: Makefile,v 1.15 2004/01/05 02:55:28 espie Exp $


PROG=	rdist
CFLAGS+=-I. -I${.CURDIR} -DOS_H=\"os-openbsd.h\"
SRCS=	gram.y child.c client.c common.c distopt.c docmd.c expand.c \
	isexec.c lookup.c message.c rdist.c
CLEANFILES+=gram.c y.tab.h

.include <bsd.prog.mk>

.if ${ELF_TOOLCHAIN:L} == "yes"
CFLAGS+=-DEXE_TYPE=EXE_ELF
.endif

.depend: gram.c
