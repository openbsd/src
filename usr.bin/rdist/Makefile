#	$OpenBSD: Makefile,v 1.14 2003/07/22 17:15:13 brad Exp $


PROG=	rdist
CFLAGS+=-I. -DOS_H=\"os-openbsd.h\"
SRCS=	gram.y child.c client.c common.c distopt.c docmd.c expand.c \
	isexec.c lookup.c message.c rdist.c
CLEANFILES+=gram.c y.tab.h

.include <bsd.prog.mk>

.if ${ELF_TOOLCHAIN:L} == "yes"
CFLAGS+=-DEXE_TYPE=EXE_ELF
.endif

.depend: gram.c
