#	$OpenBSD: Makefile,v 1.9 2002/11/18 22:49:15 henning Exp $

PROG=	pfctl
SRCS=	pfctl.c parse.y pfctl_parser.c pf_print_state.c pfctl_altq.c
CFLAGS+= -Wall -Wmissing-prototypes -Wno-uninitialized
CFLAGS+= -Wstrict-prototypes
YFLAGS=
MAN=	pfctl.8
 
LDADD+=	-lm
DPADD+=	${LIBM}

.include <bsd.prog.mk>
