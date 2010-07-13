#	$OpenBSD: Makefile,v 1.28 2010/07/13 21:31:17 nicm Exp $

# Regression tests by Niall O'Higgins <niallo@openbsd.org> and
# Tobias Stoeckmann <tobias@openbsd.org>.
# Based on OpenRCS regression framework.

CVS?=		opencvs
CVSCMD=		${CVS} -f

REGRESS_ROOT=	${.OBJDIR}/regress_cvs_root
REGRESS_WCOPY=	${.OBJDIR}/regress_cvs_wcopy
REGRESS_SEED=	${REGRESS_WCOPY}/seed

.if defined(REMOTE)
MYCVSROOT?=	localhost:${REGRESS_ROOT}
.if defined(CVS_SERVER)
TESTFILE=	$(CVS_SERVER)
.else
TESTFILE=	cvs
.endif
.else
TESTFILE=	${CVS}
MYCVSROOT?=	${REGRESS_ROOT}
.endif

DIFF=		diff -u

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
	cvs-export \
	cvs-update-with-local
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
	    ${CVSCMD} -q add seed2.txt > /dev/null 2> /dev/null
#	    eval 'test `${CVSCMD} -Q add seed2.txt | wc -l` -eq 0'

test-cvs-add-dir:
	@mkdir ${REGRESS_SEED}/subdir
	@cd ${REGRESS_SEED}; ${CVSCMD} -q add subdir > /dev/null 2> /dev/null
	@test -d ${REGRESS_SEED}/subdir/CVS && \
	    test -d ${REGRESS_ROOT}/seed/subdir

test-cvs-add-subdir_file:
	@echo "initial content of seed3.txt" > ${REGRESS_SEED}/subdir/seed3.txt
	@cd ${REGRESS_SEED}; \
	    ${CVSCMD} -q add subdir/seed3.txt > /dev/null 2> /dev/null
#	@eval 'test `${CVSCMD} -Q add seed2.txt | wc -l` -eq 0'

test-cvs-annotate:
	@cd ${REGRESS_SEED}; \
	    eval 'test `${CVSCMD} -Q annotate seed1.txt 2>/dev/null | \
	    wc -l` -eq 3'

test-cvs-annotate-rflag-inval_branch_rev:
	@cd ${REGRESS_SEED}; \
	    eval 'test `${CVSCMD} -Q annotate -r 1.2.3.4 seed1.txt 2>/dev/null \
	    | wc -l` -eq 0'

test-cvs-annotate-rflag-inval_branch_rev-fflag:
	@cd ${REGRESS_SEED}; \
	    eval 'test `${CVSCMD} -Q annotate -f -r 1.2.3.4 seed1.txt \
	    2>/dev/null | wc -l` -eq 3'

test-cvs-annotate-rflag-unknown_symbol-fflag:
	@cd ${REGRESS_SEED}; \
	    eval 'test `${CVSCMD} -Q annotate -f -r UNKNOWN seed1.txt \
	    2>/dev/null | wc -l` -eq 0'

test-cvs-checkout:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; ${CVSCMD} -Q -d ${MYCVSROOT} co seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt

test-cvs-checkout-kflag-k-rflag-HEAD:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVSCMD} -Q -d ${MYCVSROOT} co -rHEAD -kkv seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt
	@eval 'test `grep -c -e THEAD -e -kkv \
	    ${REGRESS_SEED}/CVS/Entries | wc -l` -eq 1'
	@eval 'test `grep -c Id$ \
	    ${REGRESS_SEED}/seed1.txt | wc -l` -eq 1'

test-cvs-checkout-kflag-kv-rflag-1.1:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVSCMD} -Q -d ${MYCVSROOT} co -r 1.1 -kkv seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt;
	@eval 'test `grep -c -e T1.1 -e -kkv \
	    ${REGRESS_SEED}/CVS/Entries | wc -l` -eq 1'

test-cvs-checkout-pflag:
	@eval 'test `${CVSCMD} -Q -d ${MYCVSROOT} checkout \
	    -p seed/seed1.txt 2>/dev/null | wc -l` -eq 2'

test-cvs-checkout-rflag_branch:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVSCMD} -Q -d ${MYCVSROOT} co -r FIRST_BRANCH seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt
	@eval 'test `grep -c TFIRST_BRANCH ${REGRESS_SEED}/CVS/Entries` -eq 2'

test-cvs-checkout-rflag-inval_branch_rev:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; ${CVSCMD} -Q -d ${MYCVSROOT} co -r1.2.3.4 seed
	@eval 'test ! -f ${REGRESS_SEED}/seed1.txt'

test-cvs-checkout-rflag-too_high:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; ${CVSCMD} -Q -d ${MYCVSROOT} co -r1.5 seed
	@eval 'test ! -f ${REGRESS_SEED}/seed1.txt'

test-cvs-checkout-rflag-HEAD:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVSCMD} -Q -d ${MYCVSROOT} co -rHEAD seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt;
	@eval 'test `grep -c THEAD ${REGRESS_SEED}/CVS/Entries | wc -l` -eq 1'

test-cvs-checkout-rflag-1.1:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVSCMD} -Q -d ${MYCVSROOT} co -r 1.1 seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt && test -f ${REGRESS_SEED}/seed2.txt

test-cvs-checkout-dflag:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVSCMD} -Q -d ${MYCVSROOT} co -d seed2 seed > /dev/null
	@test -d ${REGRESS_WCOPY}/seed2/CVS

test-cvs-checkout-subdir:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVSCMD} -Q -d ${MYCVSROOT} co seed/subdir > /dev/null
	@test -f ${REGRESS_SEED}/subdir/seed3.txt && \
	    test ! -f ${REGRESS_SEED}/seed2.txt

test-cvs-commit-Fflag:
	@sleep 1
	@echo "second line" >> ${REGRESS_SEED}/seed1.txt
	@echo "commit seed repo" > ${REGRESS_WCOPY}/logmsg.txt
	@cd ${REGRESS_SEED}; ${CVSCMD} commit \
	    -F ${REGRESS_WCOPY}/logmsg.txt > /dev/null
	@grep "^@commit seed repo" \
	    ${REGRESS_ROOT}/seed/seed1.txt,v > /dev/null
	@eval 'test `cat ${REGRESS_ROOT}/seed/seed1.txt,v | wc -l` -eq 60'
	@test -f ${REGRESS_ROOT}/seed/seed2.txt,v

test-cvs-commit-to_branch:
	@sleep 1
	@cd ${REGRESS_SEED}; ${CVSCMD} commit -m'first branch seed' > /dev/null
	@eval 'test `cat ${REGRESS_ROOT}/seed/seed1.txt,v | wc -l` -eq 80'
	@eval 'test `grep -c TFIRST_BRANCH ${REGRESS_SEED}/CVS/Entries` -eq 2'

test-cvs-diff:
	@sleep 1
	@echo "modified" > ${REGRESS_SEED}/seed2.txt
	@cd ${REGRESS_SEED}; eval 'test `${CVSCMD} -Q diff | wc -l` -eq 9'

test-cvs-diff-file:
	@sleep 1
	@echo "first branch line" >> ${REGRESS_SEED}/seed1.txt
	@cd ${REGRESS_SEED}; \
	    eval 'test `${CVSCMD} -Q diff seed1.txt | wc -l` -eq 7'

test-cvs-export:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir ${REGRESS_WCOPY}
	@cd ${REGRESS_WCOPY}; \
	    ${CVSCMD} -Q -d ${MYCVSROOT} export -r HEAD seed > /dev/null
	@test -f ${REGRESS_SEED}/seed1.txt

test-cvs-import-mflag:
	@cd ${.CURDIR}/import_seed; \
	    touch -t 200001010100 seed1.txt; \
	    ${CVSCMD} -Q -d ${MYCVSROOT} import \
	    -m'import seed repo' seed regress regress_import > /dev/null
	@chmod u+w ${REGRESS_ROOT}/seed/seed1.txt,v
	@perl ${.CURDIR}/setdate.pl ${REGRESS_ROOT}/seed/seed1.txt,v 1.1 \
	    2000.01.01.00.00.00
	@perl ${.CURDIR}/setdate.pl ${REGRESS_ROOT}/seed/seed1.txt,v 1.1.1.1 \
	    2000.01.01.00.00.00
	@sed "s/author `whoami`/author user/" \
	    ${REGRESS_ROOT}/seed/seed1.txt,v | \
	    ${DIFF} - ${.CURDIR}/test_${TESTFILE}_import_01_seed1.txt,v

test-cvs-initial: clean
	@${CVSCMD} -d ${MYCVSROOT} init
	@test -d ${REGRESS_ROOT}/CVSROOT

test-cvs-log:
	@cd ${REGRESS_SEED}; eval 'test `${CVSCMD} -Q log | wc -l` -eq 68'

test-cvs-release-dflag: test-cvs-checkout
	@cd ${REGRESS_WCOPY}; ${CVSCMD} -Q -d ${MYCVSROOT} release -d seed
	@test ! -d ${.OBJDIR}/regress_cvs_wcopy/seed
	@eval 'test `grep '^F' ${REGRESS_ROOT}/CVSROOT/history | wc -l` -eq 1'

test-cvs-remove-fflag:
	@cd ${REGRESS_SEED}; ${CVSCMD} -Q remove -f seed1.txt
	@test ! -f ${REGRESS_SEED}/seed1.txt

test-cvs-rlog:
	@eval 'test `${CVSCMD} -Q -d ${MYCVSROOT} rlog seed | wc -l` -eq 65'

test-cvs-status:
	@cd ${REGRESS_SEED}; eval 'test `${CVSCMD} -Q status | wc -l` -eq 18'

test-cvs-status-vflag:
	@cd ${REGRESS_SEED}; eval 'test `${CVSCMD} -Q status -v | wc -l` -eq 30'

test-cvs-tag:
	@cd ${REGRESS_SEED}; ${CVSCMD} -Q tag FIRST_TAG > /dev/null
	@grep FIRST_TAG ${REGRESS_ROOT}/seed/seed1.txt,v > /dev/null

test-cvs-tag-branch:
	@cd ${REGRESS_SEED}; ${CVSCMD} -Q tag -b FIRST_BRANCH > /dev/null
	@grep FIRST_BRANCH ${REGRESS_ROOT}/seed/seed1.txt,v > /dev/null

test-cvs-rtag-one_more_branch:
	@${CVSCMD} -d ${MYCVSROOT} -Q rtag -b SECOND_BRANCH seed > /dev/null
	@grep SECOND_BRANCH ${REGRESS_ROOT}/seed/seed2.txt,v \
	    | grep 4 > /dev/null

test-cvs-update-Aflag:
	@cd ${REGRESS_SEED}; ${CVSCMD} -Q update -A > /dev/null
	@eval 'test `grep -c -e T1 -e kkv ${REGRESS_SEED}/CVS/Entries` -eq 0'

test-cvs-update-Aflag-kflag-kv:
	@cd ${REGRESS_SEED}; ${CVSCMD} -Q update -A -kv > /dev/null
	@eval 'test `grep -c T1 ${REGRESS_SEED}/CVS/Entries` -eq 0'
	@eval 'test `grep -c kv ${REGRESS_SEED}/CVS/Entries` -eq 2'

test-cvs-update-Aflag-kflag-k-rflag-1.1:
	@cd ${REGRESS_SEED}; ${CVSCMD} -Q update -A -kk -r 1.1 > /dev/null
	@eval 'test `grep -c T1 ${REGRESS_SEED}/CVS/Entries` -eq 2'
	@eval 'test `grep -c kk ${REGRESS_SEED}/CVS/Entries` -eq 2'

test-cvs-update-Aflag-rflag-1.1:
	@cd ${REGRESS_SEED}; ${CVSCMD} -Q update -A -r 1.1 > /dev/null
	@eval 'test `grep -c T1 ${REGRESS_SEED}/CVS/Entries` -eq 2'
	@eval 'test `grep -c kk ${REGRESS_SEED}/CVS/Entries` -eq 0'

test-cvs-update-for_subdir:
	@cd ${REGRESS_SEED}; ${CVSCMD} -Q update > /dev/null
	@test -f ${REGRESS_SEED}/subdir/seed3.txt && \
	    test ! -f ${REGRESS_SEED}/seed1.txt && \
	    test ! -f ${REGRESS_SEED}/seed2.txt

test-cvs-merge:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir -p ${REGRESS_WCOPY}/a ${REGRESS_WCOPY}/b
	@cd ${REGRESS_WCOPY}/a; \
	    ${CVSCMD} -Q -d ${MYCVSROOT} co seed > /dev/null
	@cd ${REGRESS_WCOPY}/b; \
	    ${CVSCMD} -Q -d ${MYCVSROOT} co seed > /dev/null
	@echo "modified text" > ${REGRESS_WCOPY}/a/seed/seed1.txt
	@cd ${REGRESS_WCOPY}/a/seed && ${CVSCMD} -Q com -m 'a' > /dev/null
	@echo "modified text 2" > ${REGRESS_WCOPY}/b/seed/seed1.txt
	@cd ${REGRESS_WCOPY}/b/seed && ${CVSCMD} -Q up > /dev/null 2>&1
	@echo "modified text" > ${REGRESS_WCOPY}/b/seed/seed1.txt
	@cd ${REGRESS_WCOPY}/b/seed && test `${CVSCMD} -q up | wc -l` -eq 0

test-cvs-update-with-local:
	@rm -rf ${REGRESS_WCOPY}
	@mkdir -p ${REGRESS_WCOPY}/a ${REGRESS_WCOPY}/b
	@cd ${REGRESS_WCOPY}/a; \
	    ${CVSCMD} -Q -d ${MYCVSROOT} co seed > /dev/null
	@cd ${REGRESS_WCOPY}/b; \
	    ${CVSCMD} -Q -d ${MYCVSROOT} co seed > /dev/null
	@echo "new file" > ${REGRESS_WCOPY}/a/seed/test.txt
	@cd ${REGRESS_WCOPY}/a/seed && \
		${CVSCMD} -Q add test.txt > /dev/null && \
		${CVSCMD} -Q com -m 'a' > /dev/null; 
	@echo "local file" > ${REGRESS_WCOPY}/b/seed/test.txt
	@cd ${REGRESS_WCOPY}/b/seed && \
		${CVSCMD} -Q up 2>&1|grep -q '^C test.txt' && \
		grep -q '^local file' test.txt

clean:
	@rm -rf ${CLEANFILES}

.include <bsd.regress.mk>
