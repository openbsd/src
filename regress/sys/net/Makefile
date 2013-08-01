#	$OpenBSD: Makefile,v 1.4 2013/08/01 21:26:30 kettenis Exp $

.if defined(REGRESS_FULL)
SUBDIR +=	pf_divert
.endif

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
