# $OpenBSD: Makefile,v 1.16 2025/07/28 15:17:54 schwarze Exp $

REGRESS_TARGETS	 = append cond empty escname factorial
REGRESS_TARGETS	+= indir infinite startde tab TH Dd
LINT_TARGETS	 = escname indir infinite

# groff-1.23.0 defect:
# infinite recursion aborts output completely

SKIP_GROFF	 = infinite

# groff-1.23.0 regression:
#  - .Dd redefines all mdoc(7) macros even if -mdoc was explicitly
#    specified.  While it is maybe not completely clear which behaviour
#    is desirable, this is closely related to the other .Dd regressions,
#    so mark it as an regression for now rather than changing mandoc(1).

SKIP_GROFF	+= Dd_opt

# --- local rules ---

REGRESS_TARGETS	+= TH_opt Dd_opt

TH_opt.in: TH.in
	cp ${.ALLSRC} ${.TARGET}

Dd_opt.in: Dd.in
	cp ${.ALLSRC} ${.TARGET}

TH_opt.mandoc_ascii: TH_opt.in
	${MANDOC} ${MOPTS} -Ios=OpenBSD -Tascii -man TH_opt.in > ${.TARGET}

Dd_opt.mandoc_ascii: Dd_opt.in
	${MANDOC} ${MOPTS} -Ios=OpenBSD -Tascii -mdoc Dd_opt.in > ${.TARGET}

TH_opt.out_ascii: TH_opt.in
	${GROFF} -Tascii -man TH_opt.in > ${.TARGET}

Dd_opt.out_ascii: Dd_opt.in
	${GROFF} -Tascii -mdoc Dd_opt.in > ${.TARGET}

clean: local-clean

local-clean: 
	rm -f TH_opt.in Dd_opt.in

.include <bsd.regress.mk>
