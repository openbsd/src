#	$OpenBSD: Makefile,v 1.3 2007/06/06 06:21:47 niallo Exp $

# Regression tests by Niall O'Higgins <niallo@openbsd.org>.
# Based on OpenRCS regression framework.

CVS?=		opencvs
DIFF=		diff -u

CLEANFILES=	regress_cvs_root regress_cvs_wcopy importdir


# XXX - These may need to be done in order.
#	(At least start with cvs-initial.)
LTESTS= cvs-initial \
	cvs-import \
	cvs-checkout-dflag

.for t in ${LTESTS}
REGRESS_TARGETS+=test-${t}
.endfor

test-cvs-initial: clean
	@${CVS} -d ${.OBJDIR}/regress_cvs_root init

test-cvs-import:
	@mkdir -p ${.OBJDIR}/importdir
	@cp -f ${.CURDIR}/import_seed/seed* ${.OBJDIR}/importdir
	@cd ${.OBJDIR}/importdir; ${CVS} -Q		\
	    -d ${.OBJDIR}/regress_cvs_root import	\
	    -m'import seed repo' seed regress regress_`date +%Y%m%d`

test-cvs-checkout-dflag:
	@${CVS} -Q -d ${.OBJDIR}/regress_cvs_root co	\
	    -d ${.OBJDIR}/regress_cvs_wcopy seed
	@test -d ${.OBJDIR}/regress_cvs_wcopy

clean:
	@rm -rf ${CLEANFILES}

.include <bsd.regress.mk>
