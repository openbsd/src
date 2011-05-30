#	$OpenBSD: Makefile,v 1.1 2011/05/30 20:23:35 martynas Exp $

PROG = cephes
SRCS = cephes.c const.c monot.c monotl.c monotll.c polevll.c \
       testvect.c testvectl.c testvectll.c
LDADD = -lm
DPADD = ${LIBM}

.include <bsd.regress.mk>
