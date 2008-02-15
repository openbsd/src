#	$OpenBSD: Makefile,v 1.25 2008/02/15 23:26:58 xsa Exp $

# Regression tests by Niall O'Higgins <niallo@openbsd.org> and
# Tobias Stoeckmann <tobias@openbsd.org>.
# Based on OpenRCS regression framework.

CVS?=		opencvs -f

REGRESS_ROOT=	${.OBJDIR}/regress_cvs_root
REGRESS_WCOPY=	${.OBJDIR}/regress_cvs_wcopy
REGRESS_SEED=	${REGRESS_WCOPY}/seed

.if defined(REMOTE)
MYCVSROOT?=	localhost:${REGRESS_ROOT}
.else
MYCVSROOT?=	${REGRESS_ROOT}
.endif

CLEANFILES=	regress_cvs_root regress_cvs_wcopy regress_cvs_stderr \
		regress_cvs_stdout


# These must be done in order.
LTESTS= cvs-initial \
	cvs-import-mflag \
	cvs-checkout-dflag \
	cvs-checkout-rflag-HEAD \
	cvs-checkout-kflag-k-rflag-HEAD \
	cvs-checkout-rflag-inval_branch_rev \
	cvs-checkout-rflag-too_high \
	cvs-checkout \
	cvs-checkout-pflag \
	cvs-tag \
	cvs-add \
	cvs-add-dir \
	cvs-add-subdir_file \
	cvs-commit-Fflag \
	cvs-checkout \
	cvs-tag-branch \
	cvs-checkout-rflag_branch \
	cvs-diff-file \
	cvs-commit-to_branch \
	cvs-annotate \
	cvs-annotate-rflag-inval_branch_rev \
	cvs-annotate-rflag-inval_branch_rev-fflag \
	cvs-annotate-rflag-unknown_symbol-fflag \
	cvs-checkout-kflag-kv-rflag-1.1 \
	cvs-update-Aflag-kflag-kv \
	cvs-update-Aflag-kflag-k-rflag-1.1 \
	cvs-update-Aflag-rflag-1.1 \
	cvs-update-Aflag \
	cvs-status \
	cvs-diff \
	cvs-remove-fflag \
	cvs-status-vflag \
	cvs-log \
	cvs-rlog \
	cvs-rtag-one_more_branch \
	cvs-export
# Known to fail.
#	cvs-update-for_subdir
#	cvs-release-dflag

.for t in ${LTESTS}
REGRESS_TARGETS+=test-${t}
.endfor

# XXX - should not print anything
test-cvs-add:
	@echo "initial content" > ${REGRESS_SEED}/seed2.txt
	@cd ${REGRESS_SEED}; \
	    ${CVS} -q add seed2.txt > /dev/null 2> /dev/null
#	    eval 'test `${CVS} -Q add seed2.txt | wc -l` -eq 0'

test-cvs-add-dir:
	@mkdir ${REGRESS_SEED}/subdir
	@cd ${REGRESS_SEED}; ${CVS} -q add subdir > /dev/null 2> /dev/null
	@test -d ${REGRESS_SEED}/subdir/CVS && \
	    test -d ${REGRESS_ROOT}/seed/subdir

test-cvs-add-subdir_file:
	@echo "initial content of seed3.txt" > ${REGRESS_SEED}/subdir/seed3.txt
	@cd ${REGRESS_SEED}; \
	    ${CVS} -q add subdir/seed3.txt > /dev/null 2> /dev/null
#	@eval 'test `${CVS} -Q add seed2.txt | wc -l` -eq 0'

test-cvs-annotate:
	@cd ${REGRESS_SEED}; \
	    eval 'test `${CVS} -Q annotate seed1.txt 2>/dev/null | wc -l` -eq 3'

test-cvs-annotate-rflag-inval_branch_rev:
	@cd ${REGRESS_SEED}; \
	    eval 'test `${CVS} -Q annotate -r 1.2.3.4 seed1.txt 2>/dev/null \
	    | wc -l` -eq 0'

test-cvs-annotate-rflag-inval_branch_rev-fflag:
	@cd ${REGRESS_SEED}; \
	    eval 'test `${CVS} -Q annotate -f -r 1.2.3.4 seed1.txt 2>/dev/null \
	    | wc -l` -eq 3'

test-cvs-annotate-rflag-unknown_symbol-fflag:
	@cd ${REGRESS_SEED}; \
	    eval 'test `${CVS} -Q annotate -f -r UNKNOWN seed1.txt 2>/dev/null \
	    | wc -l` -eq 0'

test-cvs-checkout:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; ${CVS} -Q -d ${MYCVSROOT} co seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt

test-cvs-checkout-kflag-k-rflag-HEAD:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVS} -Q -d ${MYCVSROOT} co -rHEAD -kkv seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt
	@eval 'test `grep -c -e THEAD -e -kkv \
	    ${REGRESS_SEED}/CVS/Entries | wc -l` -eq 1'
	@eval 'test `grep -c Id$ \
	    ${REGRESS_SEED}/seed1.txt | wc -l` -eq 1'

test-cvs-checkout-kflag-kv-rflag-1.1:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVS} -Q -d ${MYCVSROOT} co -r 1.1 -kkv seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt;
	@eval 'test `grep -c -e T1.1 -e -kkv \
	    ${REGRESS_SEED}/CVS/Entries | wc -l` -eq 1'

test-cvs-checkout-pflag:
	@eval 'test `${CVS} -Q -d ${MYCVSROOT} checkout \
	    -p seed/seed1.txt 2>/dev/null | wc -l` -eq 2'

test-cvs-checkout-rflag_branch:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVS} -Q -d ${MYCVSROOT} co -r FIRST_BRANCH seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt
	@eval 'test `grep -c TFIRST_BRANCH ${REGRESS_SEED}/CVS/Entries` -eq 2'

test-cvs-checkout-rflag-inval_branch_rev:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; ${CVS} -Q -d ${MYCVSROOT} co -r1.2.3.4 seed
	@eval 'test ! -f ${REGRESS_SEED}/seed1.txt'

test-cvs-checkout-rflag-too_high:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; ${CVS} -Q -d ${MYCVSROOT} co -r1.5 seed
	@eval 'test ! -f ${REGRESS_SEED}/seed1.txt'

test-cvs-checkout-rflag-HEAD:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVS} -Q -d ${MYCVSROOT} co -rHEAD seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt;
	@eval 'test `grep -c THEAD ${REGRESS_SEED}/CVS/Entries | wc -l` -eq 1'

test-cvs-checkout-rflag-1.1:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVS} -Q -d ${MYCVSROOT} co -r 1.1 seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt && test -f ${REGRESS_SEED}/seed2.txt

test-cvs-checkout-dflag:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVS} -Q -d ${MYCVSROOT} co -d seed2 seed > /dev/null
	@test -d ${REGRESS_WCOPY}/seed2/CVS

test-cvs-checkout-subdir:
	@echo "test-cvs-checkout-subdir"
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVS} -Q -d ${MYCVSROOT} co seed/subdir > /dev/null
	@test -f ${REGRESS_SEED}/subdir/seed3.txt && \
	    test ! -f ${REGRESS_SEED}/seed2.txt

test-cvs-commit-Fflag:
	@sleep 1
	@echo "second line" >> ${REGRESS_SEED}/seed1.txt
	@echo "commit seed repo" > ${REGRESS_WCOPY}/logmsg.txt
	@cd ${REGRESS_SEED}; ${CVS} commit \
	    -F ${REGRESS_WCOPY}/logmsg.txt > /dev/null
	@grep "^@commit seed repo" \
	    ${REGRESS_ROOT}/seed/seed1.txt,v > /dev/null
	@eval 'test `cat ${REGRESS_ROOT}/seed/seed1.txt,v | wc -l` -eq 60'
	@test -f ${REGRESS_ROOT}/seed/seed2.txt,v

test-cvs-commit-to_branch:
	@sleep 1
	@cd ${REGRESS_SEED}; ${CVS} commit -m'first branch seed' > /dev/null
	@eval 'test `cat ${REGRESS_ROOT}/seed/seed1.txt,v | wc -l` -eq 80'
	@eval 'test `grep -c TFIRST_BRANCH ${REGRESS_SEED}/CVS/Entries` -eq 2'

test-cvs-diff:
	@sleep 1
	@echo "modified" > ${REGRESS_SEED}/seed2.txt
	@cd ${REGRESS_SEED}; eval 'test `${CVS} -Q diff | wc -l` -eq 9'

test-cvs-diff-file:
	@sleep 1
	@echo "first branch line" >> ${REGRESS_SEED}/seed1.txt
	@cd ${REGRESS_SEED}; \
	    eval 'test `${CVS} -Q diff seed1.txt | wc -l` -eq 7'

test-cvs-export:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVS} -Q -d ${MYCVSROOT} export -r HEAD seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt

test-cvs-import-mflag:
	@cd ${.CURDIR}/import_seed; \
	    ${CVS} -Q -d ${MYCVSROOT} import \
	    -m'import seed repo' seed regress regress_import > /dev/null
	@test -f ${REGRESS_ROOT}/seed/seed1.txt,v

test-cvs-initial: clean
	@${CVS} -d ${MYCVSROOT} init
	@test -d ${REGRESS_ROOT}/CVSROOT

test-cvs-log:
	@cd ${REGRESS_SEED}; eval 'test `${CVS} -Q log | wc -l` -eq 68'

test-cvs-release-dflag: test-cvs-checkout
	@cd ${REGRESS_WCOPY}; ${CVS} -Q -d ${MYCVSROOT} release -d seed
	@test ! -d ${.OBJDIR}/regress_cvs_wcopy/seed
	@eval 'test `grep '^F' ${REGRESS_ROOT}/CVSROOT/history | wc -l` -eq 1'

test-cvs-remove-fflag:
	@cd ${REGRESS_SEED}; ${CVS} -Q remove -f seed1.txt
	@test ! -f ${REGRESS_SEED}/seed1.txt

test-cvs-rlog:
	@eval 'test `${CVS} -Q -d ${MYCVSROOT} rlog seed | wc -l` -eq 65'

test-cvs-status:
	@cd ${REGRESS_SEED}; eval 'test `${CVS} -Q status | wc -l` -eq 18'

test-cvs-status-vflag:
	@cd ${REGRESS_SEED}; eval 'test `${CVS} -Q status -v | wc -l` -eq 30'

test-cvs-tag:
	@cd ${REGRESS_SEED}; ${CVS} -Q tag FIRST_TAG > /dev/null
	@grep FIRST_TAG ${REGRESS_ROOT}/seed/seed1.txt,v > /dev/null

test-cvs-tag-branch:
	@cd ${REGRESS_SEED}; ${CVS} -Q tag -b FIRST_BRANCH > /dev/null
	@grep FIRST_BRANCH ${REGRESS_ROOT}/seed/seed1.txt,v > /dev/null

test-cvs-rtag-one_more_branch:
	@${CVS} -d ${MYCVSROOT} -Q rtag -b SECOND_BRANCH seed > /dev/null
	@grep SECOND_BRANCH ${REGRESS_ROOT}/seed/seed2.txt,v \
	    | grep 4 > /dev/null

test-cvs-update-Aflag:
	@cd ${REGRESS_SEED}; ${CVS} -Q update -A > /dev/null
	@eval 'test `grep -c -e T1 -e kkv ${REGRESS_SEED}/CVS/Entries` -eq 0'

test-cvs-update-Aflag-kflag-kv:
	@cd ${REGRESS_SEED}; ${CVS} -Q update -A -kv > /dev/null
	@eval 'test `grep -c T1 ${REGRESS_SEED}/CVS/Entries` -eq 0'
	@eval 'test `grep -c kv ${REGRESS_SEED}/CVS/Entries` -eq 2'

test-cvs-update-Aflag-kflag-k-rflag-1.1:
	@cd ${REGRESS_SEED}; ${CVS} -Q update -A -kk -r 1.1 > /dev/null
	@eval 'test `grep -c T1 ${REGRESS_SEED}/CVS/Entries` -eq 2'
	@eval 'test `grep -c kk ${REGRESS_SEED}/CVS/Entries` -eq 2'

test-cvs-update-Aflag-rflag-1.1:
	@cd ${REGRESS_SEED}; ${CVS} -Q update -A -r 1.1 > /dev/null
	@eval 'test `grep -c T1 ${REGRESS_SEED}/CVS/Entries` -eq 2'
	@eval 'test `grep -c kk ${REGRESS_SEED}/CVS/Entries` -eq 0'

test-cvs-update-for_subdir:
	@echo "test-cvs-update-for_subdir"
	@cd ${REGRESS_SEED}; ${CVS} -Q update > /dev/null
	@test -f ${REGRESS_SEED}/subdir/seed3.txt && \
	    test ! -f ${REGRESS_SEED}/seed1.txt && \
	    test ! -f ${REGRESS_SEED}/seed2.txt

clean:
	@rm -rf ${CLEANFILES}

.include <bsd.regress.mk>
