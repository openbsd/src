#	$OpenBSD: Makefile,v 1.5 2007/10/09 12:46:29 tobias Exp $

# Regression tests by Niall O'Higgins <niallo@openbsd.org> and
# Tobias Stoeckmann <tobias@openbsd.org>.
# Based on OpenRCS regression framework.

CVS?=		opencvs
DIFF=		diff -u

CLEANFILES=	regress_cvs_root regress_cvs_wcopy regress_cvs_stderr \
		regress_cvs_stdout


# These must be done in order.
LTESTS= cvs-initial \
	cvs-import-mflag \
	cvs-checkout-rflag-HEAD \
	cvs-checkout-rflag-inval_branch_rev \
	cvs-checkout-rflag-too_high \
	cvs-checkout \
	cvs-checkout-pflag \
	cvs-tag \
	cvs-add \
	cvs-commit-Fflag \
	cvs-tag-branch \
	cvs-checkout-rflag_branch \
	cvs-commit-to_branch \
	cvs-annotate \
	cvs-annotate-rflag-inval_branch_rev \
	cvs-annotate-rflag-inval_branch_rev-fflag \
	cvs-checkout-rflag-1.1 \
	cvs-update-Aflag \
	cvs-status \
	cvs-remove-fflag \
	cvs-status-vflag
# Known to fail.
#	cvs-checkout-dflag
#	cvs-rlog
# Known to behave unconform to GNU cvs.
#	cvs-annotate-rflag-unknown_symbol-fflag
#	cvs-diff
#	cvs-diff-file
#	cvs-export
#	cvs-log

.for t in ${LTESTS}
REGRESS_TARGETS+=test-${t}
.endfor

# XXX - should not print anything
test-cvs-add:
	@echo "initial content" > ${.OBJDIR}/regress_cvs_wcopy/seed/seed2.txt
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; \
	    ${CVS} -q add seed2.txt > /dev/null 2> /dev/null
#	    eval 'test `${CVS} -Q add seed2.txt | wc -l` -eq 0'

test-cvs-annotate:
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; \
	    eval 'test `${CVS} -Q annotate seed1.txt 2>/dev/null | wc -l` -eq 3'

test-cvs-annotate-rflag-inval_branch_rev:
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; \
	    eval 'test `${CVS} -Q annotate -r 1.2.3.4 seed1.txt 2>/dev/null \
	    | wc -l` -eq 0'

test-cvs-annotate-rflag-inval_branch_rev-fflag:
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; \
	    eval 'test `${CVS} -Q annotate -f -r 1.2.3.4 seed1.txt 2>/dev/null \
	    | wc -l` -eq 3'

test-cvs-annotate-rflag-unknown_symbol-fflag:
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; \
	    eval 'test `${CVS} -Q annotate -f -r UNKNOWN seed1.txt 2>/dev/null \
	    | wc -l` -eq 0'

test-cvs-checkout:
	@rm -rf ${.OBJDIR}/regress_cvs_wcopy
	@mkdir ${.OBJDIR}/regress_cvs_wcopy
	@cd ${.OBJDIR}/regress_cvs_wcopy; \
	    ${CVS} -Q -d ${.OBJDIR}/regress_cvs_root co seed > /dev/null
	@test -f ${.OBJDIR}/regress_cvs_wcopy/seed/seed1.txt

test-cvs-checkout-pflag:
	@eval 'test `${CVS} -Q -d ${.OBJDIR}/regress_cvs_root checkout \
	    -p seed/seed1.txt 2>/dev/null | wc -l` -eq 2'

test-cvs-checkout-rflag_branch:
	@rm -rf ${.OBJDIR}/regress_cvs_wcopy
	@mkdir ${.OBJDIR}/regress_cvs_wcopy
	@cd ${.OBJDIR}/regress_cvs_wcopy; \
	    ${CVS} -Q -d ${.OBJDIR}/regress_cvs_root co -r FIRST_BRANCH seed \
	    > /dev/null
	@test -f ${.OBJDIR}/regress_cvs_wcopy/seed/seed1.txt

test-cvs-checkout-rflag-inval_branch_rev:
	@rm -rf ${.OBJDIR}/regress_cvs_wcopy
	@mkdir ${.OBJDIR}/regress_cvs_wcopy
	@cd ${.OBJDIR}/regress_cvs_wcopy; \
	    ${CVS} -Q -d ${.OBJDIR}/regress_cvs_root co -r1.2.3.4 seed
	@eval 'test ! -f ${.OBJDIR}/regress_cvs_wcopy/seed/seed1.txt'

test-cvs-checkout-rflag-too_high:
	@rm -rf ${.OBJDIR}/regress_cvs_wcopy
	@mkdir ${.OBJDIR}/regress_cvs_wcopy
	@cd ${.OBJDIR}/regress_cvs_wcopy; \
	    ${CVS} -Q -d ${.OBJDIR}/regress_cvs_root co -r1.5 seed
	@eval 'test ! -f ${.OBJDIR}/regress_cvs_wcopy/seed/seed1.txt'

test-cvs-checkout-rflag-HEAD:
	@rm -rf ${.OBJDIR}/regress_cvs_wcopy
	@mkdir ${.OBJDIR}/regress_cvs_wcopy
	@cd ${.OBJDIR}/regress_cvs_wcopy; \
	    ${CVS} -Q -d ${.OBJDIR}/regress_cvs_root co -rHEAD seed > /dev/null
	@test -f ${.OBJDIR}/regress_cvs_wcopy/seed/seed1.txt

test-cvs-checkout-rflag-1.1:
	@rm -rf ${.OBJDIR}/regress_cvs_wcopy
	@mkdir ${.OBJDIR}/regress_cvs_wcopy; \
	    cd ${.OBJDIR}/regress_cvs_wcopy; \
	    ${CVS} -Q -d ${.OBJDIR}/regress_cvs_root co -r 1.1 seed > /dev/null
	@test -f ${.OBJDIR}/regress_cvs_wcopy/seed/seed1.txt

test-cvs-checkout-dflag:
	@${CVS} -Q -d ${.OBJDIR}/regress_cvs_root co -d \
	    ${.OBJDIR}/regress_cvs_wcopy seed
	@test -d ${.OBJDIR}/regress_cvs_wcopy

test-cvs-commit-Fflag:
	@sleep 1
	@echo "second line" >> ${.OBJDIR}/regress_cvs_wcopy/seed/seed1.txt
	@echo "commit seed repo" > ${.OBJDIR}/regress_cvs_wcopy/logmsg.txt
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; ${CVS} commit \
	    -F ${.OBJDIR}/regress_cvs_wcopy/logmsg.txt > /dev/null
	@grep "^@commit seed repo" \
	    ${.OBJDIR}/regress_cvs_root/seed/seed1.txt,v > /dev/null
	@eval 'test `cat ${.OBJDIR}/regress_cvs_root/seed/seed1.txt,v | wc -l` \
	    -eq 59'

test-cvs-commit-to_branch:
	@sleep 1
	@echo "first branch line" >> ${.OBJDIR}/regress_cvs_wcopy/seed/seed1.txt
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; ${CVS} commit \
	    -m'first branch seed' > /dev/null
	@eval 'test `cat ${.OBJDIR}/regress_cvs_root/seed/seed1.txt,v | wc -l` \
	    -eq 75'

test-cvs-diff:
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; ${CVS} -Q diff

test-cvs-diff-file:
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; ${CVS} -Q diff seed1.txt

test-cvs-export:
	@mkdir ${.OBJDIR}/regress_cvs_wcopy; \
	    cd ${.OBJDIR}/regress_cvs_wcopy; \
	    ${CVS} -Q -d ${.OBJDIR}/regress_cvs_root export
	@test -f ${.OBJDIR}/regress_cvs_wcopy/seed/seed1.txt

test-cvs-import-mflag:
	@cd ${.CURDIR}/import_seed; \
	    ${CVS} -Q -d ${.OBJDIR}/regress_cvs_root import \
	    -m'import seed repo' seed regress regress_import > /dev/null
	@test -f ${.OBJDIR}/regress_cvs_root/seed/seed1.txt,v

test-cvs-initial: clean
	@${CVS} -d ${.OBJDIR}/regress_cvs_root init
	@test -d ${.OBJDIR}/regress_cvs_root/CVSROOT

test-cvs-log:
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; ${CVS} -Q log

test-cvs-remove-fflag:
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; ${CVS} -Q remove -f seed1.txt

test-cvs-rlog:
	@${CVS} -Q -d ${.OBJDIR}/regress_cvs_root rlog seed

test-cvs-status:
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; \
	    eval 'test `${CVS} -Q status | wc -l` -eq 12'

test-cvs-status-vflag:
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; \
	    eval 'test `${CVS} -Q status -v | wc -l` -eq 21'

test-cvs-tag:
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; \
	    ${CVS} -Q tag FIRST_TAG > /dev/null
	@grep FIRST_TAG ${.OBJDIR}/regress_cvs_root/seed/seed1.txt,v \
	    > /dev/null

# Does not work with OpenCVS, but we need it for further tests _now_.
test-cvs-tag-branch:
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; \
	    cvs -Q tag -b FIRST_BRANCH > /dev/null
	@grep FIRST_BRANCH ${.OBJDIR}/regress_cvs_root/seed/seed1.txt,v \
	    > /dev/null

# XXX - Output is not GNU conform
test-cvs-update-Aflag:
	@cd ${.OBJDIR}/regress_cvs_wcopy/seed; \
	    ${CVS} -Q update -A > /dev/null
#	    eval 'test `${CVS} -q update -A | wc -l` -eq 1'

clean:
	@rm -rf ${CLEANFILES}

.include <bsd.regress.mk>
