# $OpenBSD: Makefile,v 1.13 2019/04/21 23:45:50 schwarze Exp $

REGRESS_TARGETS	 = append cond empty escname factorial
REGRESS_TARGETS	+= indir infinite startde tab TH Dd
LINT_TARGETS	 = escname indir infinite

# groff-1.22.4 defect:
# infinite recursion aborts output completely

SKIP_GROFF	 = infinite

.include <bsd.regress.mk>


# --- additions to public targets ---

all ascii: ascii-diff-opt

ascii-clean: ascii-clean-opt

groff: TH.out_ascii_opt Dd.out_ascii_opt

groff-clean: groff-clean-opt


# --- local rules ---

ascii-diff-opt: TH.mandoc_ascii_opt Dd.mandoc_ascii_opt
	@${DIFF} ${.CURDIR}/TH.out_ascii_opt TH.mandoc_ascii_opt
	@${DIFF} ${.CURDIR}/Dd.out_ascii_opt Dd.mandoc_ascii_opt

TH.mandoc_ascii_opt: TH.in
	@${MANDOC} -Ios=OpenBSD -Tascii -man ${.ALLSRC} > ${.TARGET}

Dd.mandoc_ascii_opt: Dd.in
	@${MANDOC} -Ios=OpenBSD -Tascii -mdoc ${.ALLSRC} > ${.TARGET}

ascii-clean-opt:
	@rm -f TH.mandoc_ascii_opt Dd.mandoc_ascii_opt

TH.out_ascii_opt: TH.in
	/usr/local/bin/nroff -c -man -Tascii ${.ALLSRC} > ${.TARGET}

Dd.out_ascii_opt: Dd.in
	/usr/local/bin/nroff -c -mdoc -Tascii ${.ALLSRC} > ${.TARGET}

groff-clean-opt:
	rm -f TH.out_ascii_opt Dd.out_ascii_opt
