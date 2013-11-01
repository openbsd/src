#	$OpenBSD: Makefile,v 1.5 2013/11/01 18:33:10 bluhm Exp $

SUBDIR +=	pf_divert

.MAIN: regress

PROG=		pf_print_test
CLEANFILES=	pf_print_host.c
CFLAGS=		-Wall
CPPFLAGS=	-I.

pf_print_host.c: ../../../sys/net/pf.c Makefile
	sed -n '/^pf_print_host/{x;p;x;};/^pf_print_host/,/^}/p;h' \
	    ${.CURDIR}/../../../sys/net/pf.c >$@.tmp
	mv $@.tmp $@

beforedepend: pf_print_host.c

pf_print_test.o: pf_print_test.c pf_print_host.c

.include <bsd.regress.mk>
