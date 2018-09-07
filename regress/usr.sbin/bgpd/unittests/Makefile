# $OpenBSD: Makefile,v 1.1 2018/09/07 08:40:00 claudio Exp $

.PATH:		${.CURDIR}/../../../../usr.sbin/bgpd

REGRESS_TARGETS = run-regress-rde_sets_test

PROG=	rde_sets_test
SRCS=	rde_sets_test.c rde_sets.c
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../../../../usr.sbin/bgpd
LDADD= -lutil
DPADD+= ${LIBUTIL}

run-regress-rde_sets_test: ${PROG}
	./${PROG}

.include <bsd.regress.mk>
