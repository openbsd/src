#	$OpenBSD: Makefile,v 1.10 2002/12/06 16:16:15 henning Exp $

PROG=	pfctl
SRCS=	pfctl.c parse.y pfctl_parser.c pf_print_state.c pfctl_altq.c
CFLAGS+= -Wall -Wmissing-prototypes -Wno-uninitialized
CFLAGS+= -Wstrict-prototypes
YFLAGS=
MAN=	pfctl.8

LDADD+=	-lm
DPADD+=	${LIBM}

.include <bsd.prog.mk>
