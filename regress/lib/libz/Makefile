# $OpenBSD: Makefile,v 1.4 2022/03/29 23:12:30 tb Exp $

# compile and run zlib test files from upstream
# https://github.com/madler/zlib/tree/master/test

# google chromium has special test for libz bugs
# https://chromium.googlesource.com/chromium/src/third_party/+/refs/heads/main/zlib/contrib/tests/utils_unittest.cc

PROGS=		example infcover minigzip utils_unittest
SRCS_utils_unittest =	utils_unittest.cc
LDADD=		-lz
DPADD=		${LIBZ}
CPPFLAGS=	-I${.CURDIR}/../../../lib/libz
WARNINGS=	Yes
CLEANFILES=	foo.gz in out out.gz

REGRESS_TARGETS=	run-regress-example run-regress-infcover

.for stategy in default filtered huffman rle
.for level in 0 1 2 3 4 5 6 7 8 9
.for zero rand in 0 0  0 1  1 0  0 10  5 5  10 0

REGRESS_TARGETS+=	run-${stategy}-level${level}-zero${zero}-rand${rand}

run-${stategy}-level${level}-zero${zero}-rand${rand}: minigzip
	rm -f in out out.gz
	dd status=none seek=${zero} count=${rand} if=/dev/random of=in bs=1k
	./minigzip ${stategy:Ndefault:C/^([frh]).*/-\1/} ${level:N0:S/^/-/} \
	    <in >out.gz
	gunzip -k out.gz
	cmp in out

.endfor
.endfor
.endfor

.for tst in 0 1 2 3 4 5 6

REGRESS_TARGETS+=	run-utils_unittest-${tst}

run-utils_unittest-${tst}: utils_unittest
	./utils_unittest ${tst}

.endfor

.include <bsd.regress.mk>
