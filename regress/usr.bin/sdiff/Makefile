#	$OpenBSD: Makefile,v 1.2 2006/03/05 07:13:11 otto Exp $

SDIFF?=sdiff
REGRESS_TARGETS=test-lflag test-sflag test-wflag \
    test-tabs test-tabends test-tabends2 test-tabends3 \
    test-merge test-merge2 test-merge3 test-merge4 test-merge5 \
    test-merge6 test-merge7 test-merge8 test-merge9 test-merge10 \
    test-merge11 test-merge12 test-same test-oneline test-oneline2 \
    test-dot test-dot2 test-short test-stdin test-stdin2 \
    test-Iflag12 test-Iflag21 test-Isflag12 test-Isflag21 \
    test-Ilflag12 test-Ilflag21 test-Ilsflag12 test-Ilsflag21

test-Iflag12: file1 file2 Iflag12.out
	${SDIFF} -w 125 -I '.*filename.*' ${.CURDIR}/file1 ${.CURDIR}/file2 | \
	    diff -u - ${.CURDIR}/Iflag12.out

test-Iflag21: file1 file2 Iflag21.out
	${SDIFF} -w 125 -I '.*filename.*' ${.CURDIR}/file2 ${.CURDIR}/file1 | \
	    diff -u - ${.CURDIR}/Iflag21.out

test-Isflag12: file1 file2 Isflag12.out
	${SDIFF} -sw 125 -I '.*filename.*' ${.CURDIR}/file1 ${.CURDIR}/file2 | \
	    diff -u - ${.CURDIR}/Isflag12.out

test-Isflag21: file1 file2 Isflag21.out
	${SDIFF} -sw 125 -I '.*filename.*' ${.CURDIR}/file2 ${.CURDIR}/file1 | \
	    diff -u - ${.CURDIR}/Isflag21.out

test-Ilflag12: file1 file2 Ilflag12.out
	${SDIFF} -lw 125 -I '.*filename.*' ${.CURDIR}/file1 ${.CURDIR}/file2 | \
	    diff -u - ${.CURDIR}/Ilflag12.out

test-Ilflag21: file1 file2 Ilflag21.out
	${SDIFF} -lw 125 -I '.*filename.*' ${.CURDIR}/file2 ${.CURDIR}/file1 | \
	    diff -u - ${.CURDIR}/Ilflag21.out

test-Ilsflag12: file1 file2 Ilsflag12.out
	${SDIFF} -sw 125 -I '.*filename.*' ${.CURDIR}/file1 ${.CURDIR}/file2 | \
	    diff -u - ${.CURDIR}/Ilsflag12.out

test-Ilsflag21: file1 file2 Ilsflag21.out
	${SDIFF} -sw 125 -I '.*filename.*' ${.CURDIR}/file2 ${.CURDIR}/file1 | \
	    diff -u - ${.CURDIR}/Ilsflag21.out

test-lflag: file1 file2 lflag.out
	${SDIFF} -l ${.CURDIR}/file1 ${.CURDIR}/file2 | diff -u ${.CURDIR}/lflag.out -

test-sflag: file1 file2 sflag.out
	${SDIFF} -s ${.CURDIR}/file1 ${.CURDIR}/file2 | diff -u ${.CURDIR}/sflag.out -

test-wflag: file1 file2 wflag.out
	${SDIFF} -w 125 ${.CURDIR}/file1 ${.CURDIR}/file2 | diff -u ${.CURDIR}/wflag.out -

test-tabs: tabs1 tabs2 tabs.out
	${SDIFF} ${.CURDIR}/tabs1 ${.CURDIR}/tabs2 | diff -u ${.CURDIR}/tabs.out -

test-tabends: tabends tabends.out
	${SDIFF} -w30 ${.CURDIR}/tabends /dev/null | diff -u ${.CURDIR}/tabends.out -

test-tabends2: tabends tabends2.out
	${SDIFF} -w30 /dev/null ${.CURDIR}/tabends | diff -u ${.CURDIR}/tabends2.out -

test-tabends3: tabends tabends3.out
	${SDIFF} -w19 ${.CURDIR}/tabends /dev/null | diff -u ${.CURDIR}/tabends3.out -

test-merge: file1 file2
	yes l | ${SDIFF} -o merge.out ${.CURDIR}/file1 ${.CURDIR}/file2 >/dev/null || true
	diff -u ${.CURDIR}/file1 merge.out

test-merge2: file1 file2
	yes r | ${SDIFF} -o merge.out ${.CURDIR}/file1 ${.CURDIR}/file2 >/dev/null || true
	diff -u ${.CURDIR}/file2 merge.out

test-merge3: file1 file2
	yes el | VISUAL=cat ${SDIFF} -o merge.out ${.CURDIR}/file1 ${.CURDIR}/file2 >/dev/null || true
	diff -u ${.CURDIR}/file1 merge.out

test-merge4: file1 file2
	yes er | VISUAL=cat ${SDIFF} -o merge.out ${.CURDIR}/file1 ${.CURDIR}/file2 >/dev/null || true
	diff -u ${.CURDIR}/file2 merge.out

test-merge5: file1 file2
	yes l | ${SDIFF} -so merge.out ${.CURDIR}/file1 ${.CURDIR}/file2 >/dev/null || true
	diff -u ${.CURDIR}/file1 merge.out

test-merge6: file1 file2
	yes r | ${SDIFF} -so merge.out ${.CURDIR}/file1 ${.CURDIR}/file2 >/dev/null || true
	diff -u ${.CURDIR}/file2 merge.out

test-merge7: file1 file2
	yes l | ${SDIFF} -lo merge.out ${.CURDIR}/file1 ${.CURDIR}/file2 >/dev/null || true
	diff -u ${.CURDIR}/file1 merge.out

test-merge8: file1 file2
	yes r | ${SDIFF} -lo merge.out ${.CURDIR}/file1 ${.CURDIR}/file2 >/dev/null || true
	diff -u ${.CURDIR}/file2 merge.out

test-merge9: file1 file2
	yes l | ${SDIFF} -lso merge.out ${.CURDIR}/file1 ${.CURDIR}/file2 >/dev/null || true
	diff -u ${.CURDIR}/file1 merge.out

test-merge10: file1 file2
	yes r | ${SDIFF} -lso merge.out ${.CURDIR}/file1 ${.CURDIR}/file2 >/dev/null || true
	diff -u ${.CURDIR}/file2 merge.out

test-merge11: file1 file2
	{ while :; do echo s;echo l;echo v;echo l;done; } | \
	    ${SDIFF} -o merge.out ${.CURDIR}/file1 ${.CURDIR}/file2 >/dev/null || true
	diff -u ${.CURDIR}/file1 merge.out

test-merge12: file1 file2
	{ while :; do echo s;echo r;echo v;echo r;done; } | \
	    ${SDIFF} -o merge.out ${.CURDIR}/file1 ${.CURDIR}/file2 >/dev/null || true
	diff -u ${.CURDIR}/file2 merge.out

test-same: file1
	${SDIFF} ${.CURDIR}/file1 ${.CURDIR}/file1 | diff -u ${.CURDIR}/same.out -

test-oneline: oneline.in
	${SDIFF} /dev/null ${.CURDIR}/oneline.in | fgrep -q abcd

test-oneline2: oneline.in
	${SDIFF} ${.CURDIR}/oneline.in /dev/null | fgrep -q abcd

test-dot:
	${SDIFF} /dev/null ${.CURDIR}/dot.in | fgrep -q '> .'

test-dot2:
	${SDIFF} ${.CURDIR}/dot.in /dev/null | fgrep -q .

test-stdin:
	echo stdin | ${SDIFF} /dev/null /dev/stdin | fgrep -q '> stdin'

test-stdin2:
	echo stdin | ${SDIFF} /dev/stdin /dev/null | fgrep -q stdin

test-short: short.out
	echo "r\nl\nr\nl" | ${SDIFF} -o merge.out ${.CURDIR}/file1 ${.CURDIR}/file2 \
	    >/dev/null || true
	diff -u ${.CURDIR}/short.out merge.out

clean:
	rm -f merge.out

.include <bsd.regress.mk>
