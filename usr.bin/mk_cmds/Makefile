#	$OpenBSD: Makefile,v 1.1 1996/11/15 09:26:03 downsj Exp $

PROG=	mk_cmds
SRCS=	mk_cmds.c options.c utils.c ct.c cmd_tbl.c
CFLAGS+=-I. -I${.CURDIR}/../../lib/libss
LFLAGS=	-l
LDADD+=	-ll
DPADD+=	${LIBL}
CLEANFILES+=	y.tab.c y.tab.h lex.yy.c cmd_tbl.c ct.c
BINDIR=	/usr/bin
NOMAN=	noman

.include <bsd.prog.mk>
