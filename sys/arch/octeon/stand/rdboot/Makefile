#	$OpenBSD: Makefile,v 1.1 2019/07/17 14:36:32 visa Exp $

NOMAN=

.if ${MACHINE} == "octeon"
PROG=		rdboot
SRCS=		cmd.c disk.c rdboot.c vars.c
LDADD+=		-lutil
LDSTATIC+=	-static
.else
NOPROG=
.endif

install:

.include <bsd.prog.mk>
