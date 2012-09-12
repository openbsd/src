#	$OpenBSD: Makefile,v 1.4 2012/09/12 09:19:54 haesbaert Exp $

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
CDIAGFLAGS=
YFLAGS=

.include <bsd.prog.mk>
