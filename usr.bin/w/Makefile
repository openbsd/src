#	$OpenBSD: Makefile,v 1.8 2017/05/30 15:10:48 schwarze Exp $

PROG=	w
SRCS=	pr_time.c proc_compare.c w.c
MAN=	w.1 uptime.1
DPADD=	${LIBKVM}
LDADD=	-lkvm
LINKS=	${BINDIR}/w ${BINDIR}/uptime

.include <bsd.prog.mk>
