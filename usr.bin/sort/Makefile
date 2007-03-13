#	$OpenBSD: Makefile,v 1.3 2007/03/13 17:35:13 millert Exp $

PROG=	sort
SRCS=	append.c fields.c files.c fsort.c init.c msort.c sort.c tmp.c

# Work around gcc optimization bug on sh
.if (${MACHINE_ARCH} == sh)
COPTS+= -O1
.endif

.include <bsd.prog.mk>
