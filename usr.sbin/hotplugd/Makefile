# $OpenBSD: Makefile,v 1.1 2004/05/30 08:28:28 grange Exp $

PROG=	hotplugd
SRCS=	hotplugd.c
MAN=	hotplugd.8

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CLFAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

.include <bsd.prog.mk>
