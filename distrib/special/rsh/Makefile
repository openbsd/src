#	$OpenBSD: Makefile,v 1.2 1999/06/17 20:42:31 deraadt Exp $
#	$NetBSD: Makefile,v 1.13 1996/12/18 16:54:57 christos Exp $

PROG=	rsh
SRCS=	rsh.c
COPTS+=-Os

KERBEROS=no

.PATH:	${.CURDIR}/../../../usr.bin/rsh
MAN=
BINMODE=4555
BINOWN=	root

.include <bsd.prog.mk>
