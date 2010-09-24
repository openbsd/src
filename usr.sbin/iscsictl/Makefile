#	$OpenBSD: Makefile,v 1.2 2010/09/24 12:31:35 dlg Exp $

.PATH:		${.CURDIR}/../iscsid

PROG=	iscsictl
SRCS=	iscsictl.c parse.y parser.c
SRCS+=	util.c

MAN=	iscsictl.8

CFLAGS+= -Wall -Werror
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../iscsid
YFLAGS=

.include <bsd.prog.mk>
