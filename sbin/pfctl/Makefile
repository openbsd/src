#	$OpenBSD: Makefile,v 1.16 2004/06/10 19:02:16 miod Exp $

PROG=	pfctl
SRCS=	pfctl.c parse.y pfctl_parser.c pf_print_state.c pfctl_altq.c
SRCS+=	pfctl_osfp.c pfctl_radix.c pfctl_table.c pfctl_qstats.c
CFLAGS+= -Wall -Wmissing-prototypes -Wno-uninitialized
CFLAGS+= -Wstrict-prototypes -I${.CURDIR}
YFLAGS=
MAN=	pfctl.8

LDADD+=	-lm
DPADD+=	${LIBM}

.include <bsd.prog.mk>

# XXX
.if (${MACHINE_ARCH} == "m88k")
parse.o: parse.y
	${YACC.y} ${.IMPSRC}
	${COMPILE.c} -fno-schedule-insns2 -o ${.TARGET} y.tab.c
	rm -f y.tab.c
.endif
