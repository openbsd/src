#	$OpenBSD: Makefile,v 1.2 2011/06/02 21:47:40 martynas Exp $

PROG = cephes
SRCS = cephes.c const.c constll.c monot.c monotl.c monotll.c polevll.c \
       testvect.c testvectl.c testvectll.c
LDADD = -lm
DPADD = ${LIBM}

.include <bsd.regress.mk>
