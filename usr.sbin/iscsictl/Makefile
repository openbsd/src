#	$OpenBSD: Makefile,v 1.3 2012/06/26 21:09:44 claudio Exp $

.PATH:		${.CURDIR}/../iscsid

PROG=	iscsictl
SRCS=	iscsictl.c parse.y parser.c
SRCS+=	util.c

MAN=	iscsictl.8 iscsi.conf.5

CFLAGS+= -Wall -Werror
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../iscsid
YFLAGS=

.include <bsd.prog.mk>
