#	$OpenBSD: Makefile,v 1.2 2001/01/29 02:05:53 niklas Exp $
#	$NetBSD: Makefile,v 1.2 1998/01/09 08:03:55 perry Exp $
#
PROG=emuspeed
NOMAN=none

SRCS=emuspeed.c test.s mul64.s div64.s # movepto.c movepfrom.c

/* don't install: */
proginstall:

.include <bsd.prog.mk>

regress: all
	@./emuspeed
