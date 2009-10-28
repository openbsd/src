#	$OpenBSD: Makefile,v 1.1 2009/10/28 05:07:24 deraadt Exp $

COPTS+=-Os
PROG=	restore
SRCS=	main.c interactive.c restore.c dirs.c symtab.c tape.c utilities.c
MAN=

.PATH:	${.CURDIR}/../../../sbin/restore

.include <bsd.prog.mk>
