# $OpenBSD: Makefile,v 1.3 2018/09/10 20:51:59 benno Exp $

.PATH:		${.CURDIR}/../../../../usr.sbin/bgpd

PROGS += rde_sets_test
PROGS += rde_trie_test

.  for p in ${PROGS}
REGRESS_TARGETS += run-regress-$p
.endfor

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../../../../usr.sbin/bgpd
LDADD= -lutil
DPADD+= ${LIBUTIL}

SRCS_rde_sets_test=	rde_sets_test.c rde_sets.c
run-regress-rde_sets_test: rde_sets_test
	./rde_sets_test

SRCS_rde_trie_test=	rde_trie_test.c rde_trie.c util.c rde_sets.c
TRIE_TESTS=1 2 3 4

.for n in ${TRIE_TESTS}
TRIE_TARGETS+=run-regress-rde_trie_test-${n}

run-regress-rde_trie_test-${n}: rde_trie_test
	./rde_trie_test ${.CURDIR}/rde_trie_test.${n}.in \
	    ${.CURDIR}/rde_trie_test.${n}.check | \
	    diff -u ${.CURDIR}/rde_trie_test.${n}.out /dev/stdin
.endfor

run-regress-rde_trie_test: ${TRIE_TARGETS}

.include <bsd.regress.mk>
