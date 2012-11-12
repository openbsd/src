#	$OpenBSD: Makefile,v 1.16 2012/11/12 01:14:41 guenther Exp $


PROG=	rdist
CFLAGS+=-I. -I${.CURDIR} -DOS_H=\"os-openbsd.h\"
SRCS=	gram.y child.c client.c common.c distopt.c docmd.c expand.c \
	isexec.c lookup.c message.c rdist.c
CLEANFILES+=gram.c y.tab.h
WARNINGS=yes

.include <bsd.prog.mk>

.if ${ELF_TOOLCHAIN:L} == "yes"
CFLAGS+=-DEXE_TYPE=EXE_ELF
.endif

.depend: gram.c
