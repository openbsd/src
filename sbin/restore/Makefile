#	$OpenBSD: Makefile,v 1.4 1997/07/05 20:51:20 millert Exp $
#	$NetBSD: Makefile,v 1.18 1997/06/30 09:47:27 lukem Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PROG=	restore
LINKS=	${BINDIR}/restore ${BINDIR}/rrestore
CFLAGS+=-DRRESTORE
SRCS=	main.c interactive.c restore.c dirs.c symtab.c tape.c utilities.c \
	dumprmt.c
MAN=	restore.8
MLINKS+=restore.8 rrestore.8
.PATH:	${.CURDIR}/../dump

.include <bsd.prog.mk>
