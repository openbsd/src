#	$OpenBSD: Makefile,v 1.3 2005/05/27 05:19:55 hshoexer Exp $

PROG=	ipsecctl
MAN=	ipsecctl.8 ipsec.conf.5

SRCS=	ipsecctl.c pfkey.c pfkdump.c parse.y

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare

YFLAGS=

.include <bsd.prog.mk>
