#	$OpenBSD: Makefile,v 1.7 2015/10/26 08:04:10 vgross Exp $

SUBDIR +=	pf_divert pf_forward pf_fragment rdomains

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
