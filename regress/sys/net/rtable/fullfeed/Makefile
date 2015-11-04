#	$OpenBSD: Makefile,v 1.1 2015/11/04 09:45:52 mpi Exp $

PROG=	fullfeed
SRCS=	main.c util.c rtable.c


REGRESS_TARGETS+=	inet inet6

# XXX route entries are sorted because the radix tree stores dup chains
# XXX from the most specific to least specific
.SUFFIXES: .txt .sorted
.txt.sorted: 
	sort $< > $@

CLEANFILES+=		*.sorted

${REGRESS_TARGETS}: ${PROG} ${.TARGET}-fullfeed.sorted
	${PROG} ${.TARGET} ${TOPDIR}/${.TARGET}-fullfeed.txt | \
		sort | diff -u ${.TARGET}-fullfeed.sorted /dev/stdin

.include <bsd.regress.mk>
