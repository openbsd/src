# $OpenBSD: Makefile,v 1.1 2026/06/22 15:52:50 tim Exp $

REGRESS_TARGETS=	a-format concat-assign-same decr-NF fs-overflow \
			getline-corruption getline-numeric inf-nan-torture \
			matchop-deref missing-precision negative-nf \
			nf-self-assign numeric-fs numeric-output-seps \
			numeric-rs numeric-subsep ofs-rebuild pfile-overflow \
			repetition-no-atom repetition-overflow rs_underflow \
			rstart-rlength space split-fs-from-array string-conv \
			subsep-overflow system-status unary-plus \
			unicode-fs-rs-1 unicode-fs-rs-2 unicode-null-match

CLEANFILES=		*.out

.SUFFIXES: .awk

.awk:
	LC_CTYPE=en_US.UTF-8 ${AWK} -f $< `[ -f ${.CURDIR}/$@.in ] && echo \
	    ${.CURDIR}/$@.in` >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

missing-precision negative-nf pfile-overflow repetition-no-atom \
repetition-overflow:
	-${AWK} -f ${.CURDIR}/$@.awk >/dev/null 2>$@.out
	grep -qf ${.CURDIR}/$@.ok $@.out

.include <bsd.regress.mk>
