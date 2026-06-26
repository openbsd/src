# $OpenBSD: Makefile,v 1.1 2026/06/26 09:33:58 tim Exp $

REGRESS_TARGETS=	T.-f-f.1 T.-f-f.2 T.-f-f.3 T.-f-f.4 \
			T.argv.1 T.argv.2 T.argv.3 T.argv.4 T.argv.5 T.argv.6 \
			T.argv.7 T.argv.8 T.argv.9 T.argv.10 T.argv.11 \
			T.argv.12 T.argv.13 T.argv.14 T.argv.15 \
			T.builtin.1 T.builtin.2 T.builtin.3 T.builtin.4 \
			T.builtin.5 T.builtin.6 T.builtin.7 T.builtin.8 \
			T.builtin.9 T.builtin.10 T.builtin.11 \
			T.chem.lsd1 T.chem.penicil T.chem.res \
			T.close.1 T.close.2 T.close.3 T.close.4 T.close.5 \
			T.close.6 \
			T.clv.1 T.clv.2 T.clv.3 T.clv.4 T.clv.5 T.clv.6 \
			T.clv.7 T.clv.8 T.clv.9 T.clv.10 T.clv.11 T.clv.12 \
			T.clv.13 T.clv.14 T.clv.15 T.clv.16 T.clv.17 T.clv.18 \
			T.clv.19 T.clv.20 T.clv.21 T.clv.22 T.clv.23 T.clv.24 \
			T.clv.25 T.clv.26 T.clv.27 T.clv.28 \
			T.csconcat \
			T.csv \
			T.delete \
			T.errmsg.1 T.errmsg.2 T.errmsg.3 T.errmsg.4 \
			T.errmsg.5 T.errmsg.6 T.errmsg.7 T.errmsg.8 \
			T.errmsg.9 T.errmsg.10 T.errmsg.11 T.errmsg.12 \
			T.errmsg.13 T.errmsg.14 T.errmsg.15 T.errmsg.16 \
			T.errmsg.17 T.errmsg.18 T.errmsg.19 T.errmsg.20 \
			T.errmsg.21 T.errmsg.22 T.errmsg.23 T.errmsg.24 \
			T.errmsg.25 T.errmsg.26 T.errmsg.27 T.errmsg.28 \
			T.errmsg.29 T.errmsg.30 T.errmsg.31 T.errmsg.32 \
			T.errmsg.33 T.errmsg.34 T.errmsg.35 T.errmsg.36 \
			T.errmsg.37 T.errmsg.38 T.errmsg.39 T.errmsg.40 \
			T.errmsg.41 T.errmsg.42 T.errmsg.43 T.errmsg.44 \
			T.errmsg.45 T.errmsg.46 T.errmsg.47 T.errmsg.48 \
			T.errmsg.49 T.errmsg.50 T.errmsg.51 T.errmsg.52 \
			T.errmsg.53 T.errmsg.54 T.errmsg.55 T.errmsg.56 \
			T.errmsg.57 T.errmsg.58 T.errmsg.59 \
			T.expr \
			T.exprconv \
			T.flags.1 T.flags.2 T.flags.3 T.flags.4 T.flags.5 \
			T.flags.6 \
			T.func.1 T.func.2 T.func.3 T.func.4 T.func.5 T.func.6 \
			T.func.7 T.func.8 T.func.9 T.func.10 T.func.11 \
			T.func.12

REGRESS_EXPECTED_FAILURES?=	T.errmsg.49 T.errmsg.50

CLEANFILES=		*.out *.tmp T.argv.3.ok T.argv.13.in T.argv.14.ok \
			T.argv.15.1.in T.argv.15.2.in T.argv.15.3.in
TESTFILE=		${.CURDIR}/../test.data

.SUFFIXES: .awk

.awk:
	${AWK} -f $< `[ -f ${.CURDIR}/$@.in ] && echo ${.CURDIR}/$@.in` >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# Tests without explicit targets:
#
# T.csconcat: "test constant string concatenation"
# T.delete: "misc tests of array deletion"
# T.exprconv: "check conversion of expr to number"

#
# T.-f-f: "check multiple -f arguments"
#

# "multiple -fs"
T.-f-f.1:
	echo foo | ${AWK} -f ${.CURDIR}/$@.1.awk -f ${.CURDIR}/$@.2.awk >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "-f -"
T.-f-f.2:
	echo '/a/' | ${AWK} -f - ${TESTFILE} >$@.1.out
	${AWK} '/a/' ${TESTFILE} >$@.2.out
	cmp -s $@.1.out $@.2.out

# "3 files"
T.-f-f.3:
	${AWK} -f ${.CURDIR}/$@.1.awk -f ${.CURDIR}/$@.2.awk \
	    -f ${.CURDIR}/$@.3.awk ${TESTFILE} >$@.out
	grep . ${TESTFILE} | cmp - $@.out

# "source file name"
T.-f-f.4:
	-${AWK} -f ${.CURDIR}/$@.1.awk -f ${.CURDIR}/$@.2.awk \
	    -f ${.CURDIR}/$@.3.awk >/dev/null 2>$@.out
	grep -qf ${.CURDIR}/$@.ok $@.out

#
# T.argv: "misc tests of argc and argv"
#

# T.argv.1: "echo1 *"
# T.argv.2: "echo2 *"
T.argv.1 T.argv.2:
	${AWK} -f ${.CURDIR}/$@.awk foo bar baz >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "argc *"
T.argv.3:
	sed s,@AWK@,${AWK:T}, ${.CURDIR}/$@.ok.in >$@.ok
	${AWK} -f ${.CURDIR}/$@.awk a bc def gh >$@.out
	cmp -s $@.ok $@.out

# "L=foo <foo1"
T.argv.4:
	${AWK} '{ print L $$0 }' L=foo <${.CURDIR}/$@.in >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "L=foo foo1"
T.argv.5:
	${AWK} '{ print L $$0 }' L=foo ${.CURDIR}/$@.in >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "L=foo -"
T.argv.6:
	${AWK} '{ print L $$0 }' L=foo - <${.CURDIR}/$@.in >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "L=foo L=glop"
T.argv.7:
	${AWK} '{ print L $$0 }' L=foo ${.CURDIR}/$@.in L=glop \
	    ${.CURDIR}/$@.in >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "L=11 L=22"
T.argv.8:
	${AWK} '{ print L $$0 }' L=11 ${.CURDIR}/$@.in L=22 ${.CURDIR}/$@.in \
	    >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "assignment operand modified"
T.argv.9:
	${AWK} 'BEGIN { print ARGV[1] } { print ARGV[1] }' name=value \
	    ${.CURDIR}/$@.in >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "ARGV[1] + ARGV[2]"
T.argv.10:
	${AWK} 'BEGIN { print ARGV[1] + ARGV[2] }' 1 2.345 >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "ENVIRON[x1] + ENVIRON[x2]"
T.argv.11:
	x1=1 x2=2.345 ${AWK} 'BEGIN { print ENVIRON["x1"] + ENVIRON["x2"] }' \
	    1 2.345 >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "zap ARGV[2]"
T.argv.12:
	${AWK} -f ${.CURDIR}/$@.awk ${.CURDIR}/$@.1.in ${.CURDIR}/$@.2.in \
	    ${.CURDIR}/$@.3.in >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "startup FILENAME"
T.argv.13:
	echo >$@.in
	rm -f nonexistent
	${AWK} -f ${.CURDIR}/$@.awk nonexistent >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "delete ARGV[2]"
T.argv.14:
	sed s,@AWK@,${AWK:T}, ${.CURDIR}/$@.ok.in >$@.ok
	${AWK} -f ${.CURDIR}/$@.awk >$@.out
	cmp -s $@.ok $@.out

# "delete ARGV"
T.argv.15:
	echo >$@.1.in
	echo >$@.2.in
	echo >$@.3.in
	${AWK} -f ${.CURDIR}/$@.awk $@.1.in $@.2.in $@.3.in >$@.out
	test ! -s $@.out

#
# T.builtin: "test miscellaneous builtin functions"
#

# Tests without explicit targets:
#
# T.builtin.1: "index/substr"
# T.builtin.2: "sin/cos"
# T.builtin.4: "toupper/tolower"
# T.builtin.10: "length array"
# T.builtin.11: "continuation handling (backslash)"

# "rand"
T.builtin.3:
	${AWK} -f ${.CURDIR}/$@.awk
	cmp -s $@.1.out $@.2.out

# "toupper/tolower for utf-8"
T.builtin.5:
	LC_CTYPE=de_DE.UTF-8 ${AWK} -f ${.CURDIR}/$@.awk ${.CURDIR}/$@.in \
	    >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# T.builtin.6: "printf arg list not evaluated"
# T.builtin.7: "substr arg list not evaluated"
# T.builtin.8: "sub() arg list not evaluated"
T.builtin.6 T.builtin.7 T.builtin.8:
	${AWK} -f ${.CURDIR}/$@.awk

# "too many args not caught"
T.builtin.9:
	${AWK} -f ${.CURDIR}/$@.awk 2>$@.out
	grep -qf ${.CURDIR}/$@.ok $@.out

T.chem.lsd1 T.chem.penicil T.chem.res:
	${AWK} -f ${.CURDIR}/T.chem.awk ${.CURDIR}/$@.in >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

#
# T.close: test close built-in
#

T.close.1:
	rm -f $@.out
	${AWK} -f ${.CURDIR}/$@.awk ${TESTFILE}
	cmp -s ${TESTFILE} $@.out

T.close.2:
	${AWK} -f ${.CURDIR}/$@.awk ${TESTFILE}
	tail -1 ${TESTFILE} | cmp -s - $@.out

#
# T.clv: check command-line variables
#

# "stdin only, no cmdline asgn"
T.clv.1:
	${AWK} -f ${.CURDIR}/$@.awk <${.CURDIR}/$@.in >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "cmdline asgn then stdin"
T.clv.2:
	${AWK} -f ${.CURDIR}/$@.awk x=1 <${.CURDIR}/$@.in >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "several cmdline asgn, then stdin"
T.clv.3:
	${AWK} -f ${.CURDIR}/$@.awk x=1 x=2 x=3 <${.CURDIR}/$@.in >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "several cmdline asgn, then file"
T.clv.4:
	${AWK} -f ${.CURDIR}/$@.awk x=1 x=2 x=3 ${.CURDIR}/$@.in >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "cmdline asgn then file"
T.clv.5:
	${AWK} -f ${.CURDIR}/$@.awk x=4 /dev/null >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "cmdline asgn then file but no read of it"
T.clv.6:
	${AWK} -f ${.CURDIR}/$@.awk x=5 /dev/null >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "cmdline asgn then file then read"
T.clv.7:
	${AWK} -f ${.CURDIR}/$@.awk x=6 ${.CURDIR}/$@.in x=end >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "cmdline asgn then file then read"
T.clv.8:
	${AWK} -f ${.CURDIR}/$@.awk x=7 /dev/null x=end >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "cmdline asgn then file then read; _ in commandname"
T.clv.9:
	${AWK} -f ${.CURDIR}/$@.awk _=7A /dev/null _=end >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "illegal varname in commandname"
T.clv.10:
	-${AWK} '{ print }' 99_=foo /dev/null 2>$@.out
	grep -qf ${.CURDIR}/$@.ok $@.out

# "these test the new -v option:  awk ... -v a=1 -v b=2 'prog' does before
# BEGIN"

# "x=11"
T.clv.11:
	${AWK} -v x=123 'BEGIN { print x }' >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=11a"
T.clv.12:
	${AWK} -vx=123 'BEGIN { print x }' >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=12"
T.clv.13:
	${AWK} -v x=123 -v y=abc -v z1=10.99 'BEGIN { print x, y, z1 }' >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=12a" (1)
T.clv.14:
	${AWK} -vx=123 -vy=abc -vz1=10.99 'BEGIN { print x, y, z1 }' >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=12a" (2)
T.clv.15:
	${AWK} -v x=123 -v y=abc -v z1=10.99 -- 'BEGIN { print x, y, z1 }' \
	    >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=13"
T.clv.16:
	${AWK} -v x=123 -v y=abc -f ${.CURDIR}/$@.awk -v z1=10.99 >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=13a"
T.clv.17:
	${AWK} -vx=123 -vy=abc -f ${.CURDIR}/$@.awk -vz1=10.99 >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=14"
T.clv.18:
	${AWK} -f ${.CURDIR}/$@.awk -v x=123 -v y=abc -v z1=10.99 >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=14a"
T.clv.19:
	${AWK} -f ${.CURDIR}/$@.awk -vx=123 -vy=abc -vz1=10.99 >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=15"
T.clv.20:
	${AWK} -f ${.CURDIR}/$@.awk -v x=123 -v y=abc -v z1=10.99 /dev/null \
	    x=4567 /dev/null >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=15a"
T.clv.21:
	${AWK} -f ${.CURDIR}/$@.awk -vx=123 -vy=abc -vz1=10.99 /dev/null \
	    x=4567 /dev/null >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=16"
T.clv.22:
	${AWK} -v x=123 -v y=abc -v z1=10.99 -f ${.CURDIR}/$@.awk x=4567 \
	    ${TESTFILE} >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=16a"
T.clv.23:
	${AWK} -vx=123 -vy=abc -vz1=10.99 -f ${.CURDIR}/$@.awk x=4567 \
	    ${TESTFILE} >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "special chars in commandline assigned value"

# "x=17"
T.clv.24:
	echo foo | ${AWK} '{print x}' x='\141\\\\\142\\z' >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=18"
T.clv.25:
	echo foo | ${AWK} '{print x}' x='a\nz' >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

# "x=19"
T.clv.26:
	${AWK} 'BEGIN { printf("a%c%c%cz\n", "\b", "\r", "\f") }' >$@.1.out
	cmp -s ${.CURDIR}/$@.ok $@.1.out
	echo foo | ${AWK} '{print x}' x='a\b\r\fz' >$@.2.out
	cmp -s ${.CURDIR}/$@.ok $@.2.out

# "newer -v tests"

# "x=20"
T.clv.27:
	-${AWK} -vx 'BEGIN {print x}' 2>$@.out
	grep -qf ${.CURDIR}/$@.ok $@.out

# "x=20a"
T.clv.28:
	-${AWK} -v x 'BEGIN {print x}' 2>$@.out
	grep -qf ${.CURDIR}/$@.ok $@.out

#
# T.csv: "tests of csv field splitting, no embedded newlines"
# T.expr: "tests of miscellaneous expressions"
#

T.csv T.expr:
	${AWK} -v awk=${AWK} -f ${.CURDIR}/$@.awk ${.CURDIR}/$@.in

#
# T.errmsg: "check some error messages"
#

T.errmsg.1 T.errmsg.2 T.errmsg.3 T.errmsg.4 T.errmsg.5 T.errmsg.6 T.errmsg.7 \
T.errmsg.8 T.errmsg.9 T.errmsg.10 T.errmsg.11 T.errmsg.12 T.errmsg.13 \
T.errmsg.14 T.errmsg.15 T.errmsg.16 T.errmsg.17 T.errmsg.18 T.errmsg.19 \
T.errmsg.20 T.errmsg.21 T.errmsg.22 T.errmsg.23 T.errmsg.24 T.errmsg.25 \
T.errmsg.26 T.errmsg.27 T.errmsg.28 T.errmsg.29 T.errmsg.30 T.errmsg.31 \
T.errmsg.32 T.errmsg.33 T.errmsg.34 T.errmsg.35 T.errmsg.36 T.errmsg.37 \
T.errmsg.38 T.errmsg.39 T.errmsg.40 T.errmsg.41 T.errmsg.42 T.errmsg.43 \
T.errmsg.44 T.errmsg.45 T.errmsg.46 T.errmsg.47 T.errmsg.48 T.errmsg.49 \
T.errmsg.50 T.errmsg.51 T.errmsg.52 T.errmsg.53:
	-${AWK} -f ${.CURDIR}/$@.awk ${TESTFILE} 2>$@.out
	grep -qf ${.CURDIR}/$@.ok $@.out

T.errmsg.54:
	-${AWK} '{ print x }' x="`printf 'a\nb'`" ${TESTFILE} >/dev/null \
	    2>$@.out
	grep -qf ${.CURDIR}/$@.ok $@.out

T.errmsg.55 T.errmsg.56 T.errmsg.57 T.errmsg.58 T.errmsg.59:
	-${AWK} -safe -f ${.CURDIR}/$@.awk ${TESTFILE} 2>$@.out
	grep -qf ${.CURDIR}/$@.ok $@.out

#
# T.flags: "test some commandline flags"
#

T.flags.1:
	${AWK} 2>&1 | grep -q usage

T.flags.2:
	${AWK} -f 2>&1 | grep -q 'no program filename'

T.flags.3:
	rm -f nonexistent
	${AWK} -f nonexistent 2>&1 | grep -q "can't open file nonexistent"

T.flags.4:
	rm -f nonexistent
	${AWK} -fnonexistent 2>&1 | grep -q "can't open file nonexistent"

T.flags.5:
	${AWK} -zz 'BEGIN {}' 2>&1 | grep -q 'unknown option -zz ignored'

T.flags.6:
	${AWK} -F 2>&1 | grep -q 'no field separator'

#
# T.func: "test user-defined functions"
#

# Tests without explicit targets:
#
# T.func.1: "function return type"
# T.func.2: "array type"
# T.func.4: "convert arg to array"
# T.func.5: "factorial"
# T.func.6: "ackermann"
# T.func.7: "fib"
# T.func.8: "return"
# T.func.9: "eqn"
# T.func.10: "exit in function"
# T.func.11: "multi-dim subscript"
# T.func.12: "fall off end"

# "argument passing"
T.func.3:
	${AWK} -f ${.CURDIR}/$@.awk /dev/null >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out


.include <bsd.regress.mk>
