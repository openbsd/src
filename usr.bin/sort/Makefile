#	$OpenBSD: Makefile,v 1.4 2012/09/10 04:03:34 deraadt Exp $

PROG=	sort
SRCS=	append.c fields.c files.c fsort.c init.c msort.c sort.c tmp.c

# Work around gcc optimization bug on sh
.if (${MACHINE_ARCH} == sh)
COPTS+= -O0
.endif

.include <bsd.prog.mk>
