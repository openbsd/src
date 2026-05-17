#	$OpenBSD: Makefile,v 1.24 2026/05/17 14:11:57 sashan Exp $

PROG=	pfctl
SRCS=	pfctl.c parse.y pfctl_parser.c pf_print_state.c
SRCS+=	pfctl_osfp.c pfctl_radix.c pfctl_table.c
SRCS+=	pfctl_optimize.c pf_ruleset.c pfctl_queue.c
CFLAGS+= -Wall -Wmissing-prototypes
CFLAGS+= -Wstrict-prototypes -I${.CURDIR}
YFLAGS=
MAN=	pfctl.8

# Ruleset and Anchor handling
.PATH: ${.CURDIR}/../../sys/net

LDADD+=	-lm
DPADD+=	${LIBM}

.include <bsd.prog.mk>
