#	$OpenBSD: Makefile,v 1.3 1996/09/01 15:27:28 deraadt Exp $
#	$NetBSD: Makefile,v 1.14 1995/03/18 14:59:39 cgd Exp $

PROG=	restore
LINKS=	${BINDIR}/restore ${BINDIR}/rrestore
CFLAGS+=-DRRESTORE
SRCS=	main.c interactive.c restore.c dirs.c symtab.c tape.c utilities.c \
	dumprmt.c
MAN=	restore.8
MLINKS+=restore.8 rrestore.8
.PATH:	${.CURDIR}/../dump

.include <bsd.prog.mk>
