# $OpenBSD: Makefile,v 1.1 2006/06/02 01:07:44 ian Exp $

FILE=file

# temporarily excluded - t23
REGRESS_TARGETS=t0 t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t15 t17 t18 t20 t21 t22 t30

# .in: input file
# .out: desired result

# t0 empty
# t1 archive
# t2 ascii archive
# t3 ranlib
# t4 awk
# t5 awk
# t6 csh
# t7 awk
# t8 interpreter
# t9 java class
# t10 java serialized data
# t11 mach-o binary
# t15 tar archive
# t17 C program
# t20 HTML
# t21 SGML
# t22 XML
# t23 raw xml

all: ${REGRESS_TARGET}

.SUFFIXES: .in

.in:
	@echo ${*}
	@${FILE} ${.CURDIR}/${*}.in | \
		sed -e "s@${.CURDIR}/@@" | \
		diff - ${.CURDIR}/${*}.out || \
		(echo "XXX ${*} failed" && false)

clean:

.include <bsd.regress.mk>
