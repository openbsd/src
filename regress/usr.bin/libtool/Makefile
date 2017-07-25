# $OpenBSD: Makefile,v 1.36 2017/07/25 12:49:29 bluhm Exp $
REGRESS_TARGETS = test-compile-0 test-link-0 test-install-0 \
	test-run-0 test-link-1 test-install-1 test-run-1 \
	test-link-2 test-link-3 test-link-4 \
	test-link-5 test-link-6 test-run-2 test-run-3 \
	error-1 error-2 error-3 test-alternate-0 \
	test-implicit-0 test-run-4 test-implicit-1 \
	test-help error-4 error-6 test-bond test-implicit-2 \
	test-Wc test-Xcompiler stupid-gdb pthread-0 pthread-1 \
	pthread-2 test-o-0

COMPILE_TESTS = compile-mode-0 compile-mode-1 compile-mode-2 \
	compile-mode-3 compile-mode-4 compile-mode-5 \
	compile-mode-6 compile-mode-7 compile-mode-8 \
	compile-mode-9 compile-mode-10 compile-mode-11 \
	compile-mode-12 compile-mode-13

FAILING_TARGETS += error-0
REGRESS_TARGETS += version-override-0 version-override-1 error-5 \
		    ${COMPILE_TESTS} prefer-static linkception-0 \
		    release-0 release-1 trip-0
FAILING_TARGETS += test-all-static

REGRESS_TARGETS += ${FAILING_TARGETS}

${FAILING_TARGETS}:
	# regress target $@ is know to fail
	@echo DISABLED

compile-tests: ${COMPILE_TESTS}

LIBTOOL ?= /usr/bin/libtool

DEST = ${.OBJDIR}/dest/usr/local
DEST2 = ${.OBJDIR}/dest2/usr/local
DEST3 = ${.OBJDIR}/dest3/usr/local

WEIRD = a++b.weird
NOTWEIRD = a__b_weird

test-compile-0: ${OBJLA}

test-link-0: liba.la

test-link-1: p1

test-run-0: p1
	./p1

test-link-2: liba0.la liba1.la

test-link-3: p2

test-run-2: p2
	./p2

test-install-0: ${DEST}/lib/liba.la

test-install-1: ${DEST}/bin/p1

test-run-1: ${DEST}/bin/p1
	LD_LIBRARY_PATH=${DEST}/lib ${DEST}/bin/p1

test-run-3:  ${DEST}/bin/p2
	LD_LIBRARY_PATH=${DEST}/lib ${DEST}/bin/p2

test-run-4: p2
	${LIBTOOL} --mode=execute ./p2

test-help:
	${LIBTOOL} --help > /dev/null

# affects sysutils/nut
test-all-static: s1
	file ${.OBJDIR}/.libs/s1 | grep 'statically linked'

s1: c.lo liba.la
	${LIBTOOL} --mode=link ${CC} ${CFLAGS} -o s1 c.lo -la -all-static

error-0: liba.la
# this should error out (not absolute directory)
	if ${LIBTOOL} --mode=install cp liba.la dest; then exit 1; fi

test-bond: dummy3.c
	# one single instance of -o
	@if ${LIBTOOL} --mode=compile cc -c -o dummy3.lo -o 7 dummy3.c; then exit 1; fi

dummy.c dummy2.c dummy3.c:
	@touch $@

dummy2.lo: dummy2.c
	${LIBTOOL} --mode=compile --tag=disable-shared --tag=disable-static ${CC} -c -o dummy2.lo dummy2.c

error-1: dummy.c
	${LIBTOOL} --mode=compile --tag=notatag ${CC} -c -o dummy.lo dummy.c 2>&1|fgrep -q "ignoring unknown tag"

error-2: dummy.c
	if ${LIBTOOL} --mode=compile --tag=@ ${CC} -c -o dummy.lo dummy.c; then exit 1; fi
	
error-3: dummy.c
	if ${LIBTOOL} --mode=foo ${CC} -c -o dummy.lo dummy.c; then exit 1; fi
	
error-4: dummy.c
	# libtool -mode=... is an invalid option
	@if ${LIBTOOL} -mode=compile ${CC} -c -o dummy.lo dummy.c; then exit 1; fi

error-5: libbad.la
	fgrep -q "library_names=''" libbad.la || exit 1

error-6:
	# implicit mode detection failure: libtool foo
	@if ${LIBTOOL} foo; then exit 1; fi

prefer-static: dummy2.lo
	if fgrep -q .libs/dummy2.o dummy2.lo; then exit 1; fi

test-alternate-0: dummy.c
	${LIBTOOL} compile ${CC} -c -o dummy.lo dummy.c
	${LIBTOOL} comp ${CC} -c -o dummy.lo dummy.c

test-implicit-0: dummy.c
	${LIBTOOL} ${CC} -c -o dummy.lo dummy.c

test-implicit-1: liba.la
	mkdir -p ${DEST2}/lib
	${LIBTOOL} install liba.la ${DEST2}/lib/liba.la

test-implicit-2: liba.la
	mkdir -p ${DEST3}/lib
	${LIBTOOL} install -c liba.la ${DEST3}/lib/liba.la

version-override-0: liba9.la
	fgrep -q "liba9.so.4.2" liba9.la

version-override-1: lib${WEIRD}.la
	fgrep -q "lib${WEIRD}.so.5.6" lib${WEIRD}.la

compile-mode-0: dummy.c
	${LIBTOOL} --mode=compile ${CC} -c dummy.c|tee out0
	@fgrep .libs/dummy.o out0|fgrep -q -- -fPIC
	@fgrep -q ' dummy.o' out0
	@if fgrep ' dummy.o' out0|fgrep -q -- -fPIC; then exit 1; fi

compile-mode-1: dummy.c
	${LIBTOOL} --mode=compile --tag=disable-shared ${CC} -c dummy.c|tee out1
	@if fgrep -q .libs/dummy.o out1; then exit 1; fi
	@fgrep -q ' dummy.o' out1
	@if fgrep ' dummy.o' out1|fgrep -q -- -fPIC; then exit 1; fi

compile-mode-2: dummy.c
	${LIBTOOL} --mode=compile --tag=disable-static ${CC} -c dummy.c|tee out2
	@fgrep .libs/dummy.o out2|fgrep -q -- -fPIC
	@if fgrep -q ' dummy.o' out2; then exit 1; fi

compile-mode-3: dummy.c
	# can't disable both, will keep static
	${LIBTOOL} --mode=compile --tag=disable-static --tag=disable-shared ${CC} -c dummy.c|tee out3
	@if fgrep -q .libs/dummy.o out3; then exit 1; fi
	@fgrep -q ' dummy.o' out3
	@if fgrep ' dummy.o' out3|fgrep -q -- -fPIC; then exit 1; fi

compile-mode-4: dummy.c
	# can't disable both, will keep static
	${LIBTOOL} --mode=compile --tag=disable-shared --tag=disable-static ${CC} -c dummy.c|tee out4
	@if fgrep -q .libs/dummy.o out4; then exit 1; fi
	@fgrep -q ' dummy.o' out4
	@if fgrep ' dummy.o' out4|fgrep -- -fPIC; then exit 1; fi

compile-mode-5: dummy.c
	${LIBTOOL} --mode=compile ${CC} -static -c dummy.c|tee out5
	@if fgrep -q .libs/dummy.o out5; then exit 1; fi
	@fgrep -q ' dummy.o' out5
	@if fgrep ' dummy.o' out5|fgrep -- -fPIC; then exit 1; fi

compile-mode-6: dummy.c
	${LIBTOOL} --mode=compile ${CC} -static -prefer-pic -c dummy.c|tee out6
	@if fgrep -q .libs/dummy.o out6; then exit 1; fi
	@fgrep ' dummy.o' out6|fgrep -q -- -fPIC

compile-mode-7: dummy.c
	${LIBTOOL} --mode=compile ${CC} -static -fpie -c dummy.c|tee out7
	@if fgrep -q .libs/dummy.o out7; then exit 1; fi
	@fgrep ' dummy.o' out7|fgrep -q -- -fpie
	@if fgrep ' dummy.o' out7|fgrep -- -fpie|fgrep -q -- -fPIC; then exit 1; fi

compile-mode-8: dummy.c
	${LIBTOOL} --mode=compile ${CC} -static -prefer-pic -fpie -c dummy.c|tee out8
	@if fgrep -q .libs/dummy.o out8; then exit 1; fi
	@fgrep ' dummy.o' out8|fgrep -q -- -fPIC
	@if fgrep ' dummy.o' out8|fgrep -- -fPIC|fgrep -q -- -fpie;  then exit 1; fi

compile-mode-9: dummy.c
	${LIBTOOL} --mode=compile ${CC} -shared -c dummy.c|tee out9
	@fgrep -q .libs/dummy.o out9

compile-mode-10: dummy.c
	${LIBTOOL} --mode=compile ${CC} -shared -prefer-non-pic -c dummy.c|tee out10
	@fgrep .libs/dummy.o out10|fgrep -q -- -fPIC

compile-mode-11: dummy.c
	# -shared + -static -> static
	${LIBTOOL} --mode=compile ${CC} -shared -static -c dummy.c|tee out11
	@if fgrep -q .libs/dummy.o out11; then exit 1; fi
	@fgrep -q ' dummy.o' out11
	@if fgrep ' dummy.o' out11|fgrep -q -- -fPIC; then exit 1; fi

compile-mode-12: dummy.c
	# -shared + -static -> static
	${LIBTOOL} --mode=compile ${CC} -shared -static -prefer-pic -c dummy.c|tee out12
	@if fgrep -q .libs/dummy.o out12; then exit 1; fi
	@fgrep ' dummy.o' out12|fgrep -q -- -fPIC

compile-mode-13: dummy.c
	# -static + -shared ->  configuration error
	if ${LIBTOOL} --mode=compile ${CC} -static -shared -c dummy.c; then exit 1; fi

test-Wc: 
	${LIBTOOL} --mode=compile ${CC} -c -Wc,-DA,-DB ${.CURDIR}/d.c

test-Xcompiler: 
	${LIBTOOL} --mode=compile ${CC} -c -Xcompiler -DA -Xcompiler -DB ${.CURDIR}/d.c

stupid-gdb:
	@${LIBTOOL} --config|fgrep -q 'objdir=.libs'

linkception-0: e.lo
	# check that linking with libX11 works, which is funny to do
	# since -lX11 brings in -lxcb, BUT -lxcb wants some other stuff
	# afterwards !
	${LIBTOOL} --mode=link ${CC} -o e -L/usr/X11R6/lib e.lo -lX11

test-link-4: e.lo
	@if ${LIBTOOL} --mode=link ${CC} -o e -L /usr/X11R6/lib e.lo -lX11; then exit 1; fi

# check that -x c works with libraries linking
test-link-5: liba4.la

# but not with programs !!!
test-link-6: e.lo
	@if ${LIBTOOL} --mode=link ${CC} -o e -x c e.lo 2>/dev/null; then exit 1; fi

test-o-0: e.lo
	@if ${LIBTOOL} --mode=link ${CC} -oe e.lo; then exit 1; fi
	
pthread-0: e.lo liba2.la
	@${LIBTOOL} --mode=link ${CC} -o e e.lo -la2|tee out.13
	@fgrep -q -- -pthread out.13

pthread-1: e.lo liba2.la
	@${LIBTOOL} --mode=link ${CC} -pthread -o e e.lo -la2|tee out.14
	@fgrep -q -- -pthread out.14
	@if grep -q -- "-pthread.*-pthread" out.14; then exit 1; fi

pthread-2: e.lo
	${LIBTOOL} --mode=link ${CC} -o e -pthread -pthread e.lo |tee out.15
	@fgrep -q -- -pthread out.15
	@if grep -q -- "-pthread.*-pthread" out.15; then exit 1; fi

release-0: liba3.la
	test -f .libs/liba3-0.so.0.0 || exit 1

release-1: ${DEST}/lib/liba3.la
	@test -h ${DEST}/lib/liba3.so.0.0
	@test `readlink ${DEST}/lib/liba3.so.0.0` = liba3-0.so.0.0

trip-0: dummy.c a.lo b.lo
	rm -f .libs/libtrip.so.10.5
	mkdir -p lib1
	${CC} -shared -o  lib1/libtrip.so.10.5 dummy.c
	${CC} -shared -o  lib1/libtrip.so.10.10 dummy.c
	${LIBTOOL} --mode=link ${CC} -o libbogus.la -Llib1 ${SOPTS} a.lo b.lo -ltrip
	if test -e .libs/libtrip.so.10.5; then exit 1; fi

#weird-shit-1: a.lo
#	${LIBTOOL} --mode=link ${CC} -rpath /usr/local/lib --export-symbols a.def -o liba2.la a.lo
#
${DEST}/bin/p1: ${DEST}/lib/liba.la
${DEST}/bin/p2: ${DEST}/lib/liba0.la ${DEST}/lib/liba1.la

# basic framework to build/link stuff
SOPTS = -version-info 0:0:0 -rpath /usr/local/lib

LIBS = a a0 a1 a2 a3 a4 bad a9 ${WEIRD}
PRGS = p1 p2

OBJ_a = a.lo b.lo
a_OPTS = ${SOPTS}

OBJ_a0 = a.lo
a0_OPTS = ${SOPTS}

OBJ_a1 = b.lo
a1_OPTS = ${SOPTS} -la0

OBJ_a2 = a.lo b.lo
a2_OPTS = ${OPTS} -pthread

OBJ_a3 = a.lo b.lo
a3_OPTS = ${a_OPTS} -release 0

OBJ_a4 = a.lo b.lo
a4_OPTS = -x c ${a_OPTS} 

# libbad has a bogus rpath option
OBJ_bad = a.lo b.lo
bad_OPTS = --version-info 0:0:0 --rpath /usr/local/lib

OBJ_a9 = ${OBJ_a}
a9_OPTS = ${a_OPTS}
LIBS_ENV = liba9_ltversion=4.2

OBJ_${WEIRD} = ${OBJ_a}
${WEIRD}_OPTS = ${a_OPTS}
LIBS_ENV += lib${NOTWEIRD}_ltversion=5.6

OBJ_p1 = c.lo liba.la
LINK_p1 = c.lo -la

OBJ_p2 = c.lo liba0.la liba1.la
LINK_p2 = c.lo -la1


# stuff to compile/link/install everything
.SUFFIXES: .lo

.c.lo:
	${LIBTOOL} --mode=compile ${CC} -c ${CFLAGS} ${.CURDIR}/$*.c

.for p in ${PRGS}
$p: ${OBJ_$p}
	${LIBTOOL} --mode=link ${CC} ${CFLAGS} -o $@ ${LINK_$p}
.endfor

.for t in ${PRGS}
${DEST}/bin/$t: $t
	mkdir -p ${DEST}/bin
	${LIBTOOL} --mode=install cp $t ${DEST}/bin/$t
.endfor

.for t in ${LIBS}
${DEST}/lib/lib$t.la: lib$t.la
	mkdir -p ${DEST}/lib
	${LIBTOOL} --mode=install cp lib$t.la ${DEST}/lib/lib$t.la
.endfor

.for l in ${LIBS}
lib$l.la: ${OBJ_$l}
	${LIBS_ENV} ${LIBTOOL} --mode=link ${CC} ${CFLAGS} -o $@ ${$l_OPTS} ${OBJ_$l}
.endfor

CLEANFILES += ${PRGS} s1 e dummy*.c *.d *.o *.lo *.la .libs/* lib1/* out*
CLEANFILES += ${DEST}/bin/* ${DEST}/lib/* ${DEST2}/lib/* ${DEST3}/lib/*

.PHONY: ${REGRESS_TARGETS} regress compile-tests

.include <bsd.regress.mk>
