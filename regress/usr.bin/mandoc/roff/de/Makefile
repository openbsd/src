# $OpenBSD: Makefile,v 1.15 2022/08/02 10:59:51 schwarze Exp $

REGRESS_TARGETS	 = append cond empty escname factorial
REGRESS_TARGETS	+= indir infinite startde tab TH Dd
LINT_TARGETS	 = escname indir infinite

# groff-1.22.4 defect:
# infinite recursion aborts output completely

SKIP_GROFF	 = infinite


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
