# $OpenBSD: Makefile,v 1.1 2018/07/11 20:24:35 schwarze Exp $

LAM		= LC_CTYPE=en_US.UTF-8 lam
REGRESS_TARGETS	= noarg fonly Fback pminus stdio
CLEANFILES	= ${REGRESS_TARGETS:C/^/out_/:C/$/.txt/}

noarg:
	${LAM} ${.CURDIR}/in1.txt ${.CURDIR}/in2.txt ${.CURDIR}/in3.txt > \
		out_noarg.txt
	diff -u ${.CURDIR}/expect_noarg.txt out_noarg.txt

# also test padding (in1), truncation (in2), and back to default
fonly:
	${LAM} -f 9 ${.CURDIR}/in1.txt -f .9 ${.CURDIR}/in2.txt \
		${.CURDIR}/in3.txt > out_fonly.txt
	diff -u ${.CURDIR}/expect_fonly.txt out_fonly.txt

# also test 0 padding, and padding combined with truncation 
Fback:
	${LAM} -F 010 ${.CURDIR}/in1.txt ${.CURDIR}/in2.txt \
		-f 7.6 ${.CURDIR}/in3.txt > out_Fback.txt
	diff -u ${.CURDIR}/expect_Fback.txt out_Fback.txt

# also test separator, left alignment, and column padding
pminus:
	${LAM} -S '|' -F 9.9 ${.CURDIR}/in1.txt -P -9.9 ${.CURDIR}/in2.txt \
		${.CURDIR}/in3.txt > out_pminus.txt
	diff -u ${.CURDIR}/expect_pminus.txt out_pminus.txt

# also test line ending and separators
stdio:
	printf "11:12;21:22;" | ${LAM} -t ':' - -S= -t ';' - -s '%' > \
		out_stdio.txt
	diff -u ${.CURDIR}/expect_stdio.txt out_stdio.txt

.include <bsd.regress.mk>
