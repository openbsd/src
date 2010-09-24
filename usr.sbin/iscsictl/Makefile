#	$OpenBSD: Makefile,v 1.1 2010/09/24 09:45:17 claudio Exp $

.PATH:		${.CURDIR}/../iscsid

PROG=	iscsictl
SRCS=	iscsictl.c parse.y parser.c
SRCS+=	util.c

MAN=

CFLAGS+= -Wall -Werror
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../iscsid
YFLAGS=

.include <bsd.prog.mk>
