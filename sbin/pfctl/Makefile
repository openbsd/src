#	$OpenBSD: Makefile,v 1.14 2004/01/05 02:55:28 espie Exp $

PROG=	pfctl
SRCS=	pfctl.c parse.y pfctl_parser.c pf_print_state.c pfctl_altq.c pfctl_osfp.c
SRCS+=	pfctl_radix.c pfctl_table.c pfctl_qstats.c
CFLAGS+= -Wall -Wmissing-prototypes -Wno-uninitialized
CFLAGS+= -Wstrict-prototypes -I${.CURDIR}
YFLAGS=
MAN=	pfctl.8

LDADD+=	-lm
DPADD+=	${LIBM}

.include <bsd.prog.mk>
