#	$OpenBSD: Makefile,v 1.2 2006/07/25 09:44:49 henning Exp $

.PATH:		${.CURDIR}/../ospfd

PROG=	ospfctl
SRCS=	buffer.c imsg.c log.c ospfctl.c parser.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../ospfd
MAN=	ospfctl.8

.include <bsd.prog.mk>
