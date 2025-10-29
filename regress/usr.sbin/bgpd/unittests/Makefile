# $OpenBSD: Makefile,v 1.14 2025/10/29 13:15:22 claudio Exp $

.PATH:		${.CURDIR}/../../../../usr.sbin/bgpd

PROGS += rde_sets_test
PROGS += rde_trie_test
PROGS += rde_community_test
PROGS += rde_decide_test
PROGS += rde_aspa_test
PROGS += rde_flowspec_test
PROGS += chash_sub_test
PROGS += chash_test

.for p in ${PROGS}
REGRESS_TARGETS += run-regress-$p
.endfor

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../../../../usr.sbin/bgpd
LDADD= -lutil
DPADD+= ${LIBUTIL}

SRCS_rde_aspa_test=	rde_aspa_test.c monotime.c

SRCS_rde_sets_test=	rde_sets_test.c rde_sets.c timer.c log.c monotime.c

SRCS_rde_trie_test=	rde_trie_test.c rde_trie.c util.c rde_sets.c timer.c log.c monotime.c
run-regress-rde_trie_test:
	# cannot run without parameter

TRIE_TESTS=1 2 3 4 5 6
TRIE4_FLAGS=-o
TRIE5_FLAGS=-r
TRIE6_FLAGS=-r

.for n in ${TRIE_TESTS}
REGRESS_TARGETS+=run-regress-rde_trie_test-${n}
run-regress-rde_trie_test-${n}: rde_trie_test
	./rde_trie_test ${TRIE${n}_FLAGS} ${.CURDIR}/rde_trie_test.${n}.in \
	    ${.CURDIR}/rde_trie_test.${n}.check | \
	    diff -u ${.CURDIR}/rde_trie_test.${n}.out /dev/stdin
.endfor

SRCS_rde_community_test=	rde_community_test.c rde_community.c chash.c

SRCS_rde_decide_test=	rde_decide_test.c rde_decide.c rde_attr.c util.c chash.c

SRCS_rde_flowspec_test=	rde_flowspec_test.c flowspec.c util.c

SRCS_chash_sub_test=	chash_sub_test.c
SRCS_chash_test=	chash_test.c chash.c

.include <bsd.regress.mk>
