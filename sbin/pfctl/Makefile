#	$OpenBSD: Makefile,v 1.13 2003/08/21 19:12:08 frantzen Exp $

PROG=	pfctl
SRCS=	pfctl.c parse.y pfctl_parser.c pf_print_state.c pfctl_altq.c pfctl_osfp.c
SRCS+=	pfctl_radix.c pfctl_table.c pfctl_qstats.c
CFLAGS+= -Wall -Wmissing-prototypes -Wno-uninitialized
CFLAGS+= -Wstrict-prototypes
YFLAGS=
MAN=	pfctl.8

LDADD+=	-lm
DPADD+=	${LIBM}

.include <bsd.prog.mk>
