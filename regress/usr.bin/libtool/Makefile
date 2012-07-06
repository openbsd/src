# $OpenBSD: Makefile,v 1.14 2012/07/06 19:16:52 espie Exp $
REGRESS_TARGETS = test-compile-0 test-link-0 test-install-0 \
	test-run-0 test-link-1 test-install-1 test-run-1 \
	test-link-2 test-link-3 test-run-2 test-run-3 \
	error-0 error-1 error-2 error-3 error-4 test-alternate-0 \
	test-implicit-0 test-run-4 \
	test-implicit-1

.include <bsd.own.mk>
.if defined(NOPIC)
REGRESS_TARGETS += static-arch-fail
.else
REGRESS_TARGETS += test-all-static error-5 \
	version-override-0 version-override-1
.endif

LIBTOOL ?= /usr/bin/libtool

DEST = ${.OBJDIR}/dest/usr/local
DEST2 = ${.OBJDIR}/dest2/usr/local

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
	${LIBTOOL} --mode=execute p2

# affects sysutils/nut
test-all-static: s1
	file s1 | grep 'statically linked'

s1: c.lo liba.la
	${LIBTOOL} --mode=link ${CC} ${CFLAGS} -o s1 c.lo -la -all-static

error-0: liba.la
# this should error out (not absolute directory)
	if ${LIBTOOL} --mode=install cp liba.la dest; then exit 1; fi

dummy.c:
	@touch dummy.c

error-1: dummy.c
	${LIBTOOL} --mode=compile --tag=notatag ${CC} -c -o dummy.lo dummy.c 2>&1|fgrep -q "ignoring unknown tag"

error-2: dummy.c
	if ${LIBTOOL} --mode=compile --tag=@ ${CC} -c -o dummy.lo dummy.c; then exit 1; fi
	
error-3: dummy.c
	if ${LIBTOOL} --mode=foo ${CC} -c -o dummy.lo dummy.c; then exit 1; fi
	
error-4: dummy.c
	if ${LIBTOOL} -mode=compile ${CC} -c -o dummy.lo dummy.c; then exit 1; fi

error-5: libbad.la
	fgrep -q "library_names=''" libbad.la || exit 1

test-alternate-0: dummy.c
	${LIBTOOL} compile ${CC} -c -o dummy.lo dummy.c
	${LIBTOOL} comp ${CC} -c -o dummy.lo dummy.c

test-implicit-0: dummy.c
	${LIBTOOL} ${CC} -c -o dummy.lo dummy.c

test-implicit-1: liba.la
	mkdir -p ${DEST2}/lib
	${LIBTOOL} install liba.la ${DEST2}/lib/liba.la

version-override-0: liba9.la
	fgrep -q "liba9.so.4.2" liba9.la

version-override-1: lib${WEIRD}.la
	fgrep -q "lib${WEIRD}.so.5.6" lib${WEIRD}.la

static-arch-fail:
	@echo "Some tests are not run on static architectures"
	@exit 1

${DEST}/bin/p1: ${DEST}/lib/liba.la
${DEST}/bin/p2: ${DEST}/lib/liba0.la ${DEST}/lib/liba1.la

# basic framework to build/link stuff
SOPTS = --version-info 0:0:0 -rpath /usr/local/lib

LIBS = a a0 a1 bad a9 ${WEIRD}
PROGS = p1 p2

OBJ_a = a.lo b.lo
a_OPTS = ${SOPTS}

OBJ_a0 = a.lo
a0_OPTS = ${SOPTS}

OBJ_a1 = b.lo
a1_OPTS = ${SOPTS} -la0

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

.for p in ${PROGS}
$p: ${OBJ_$p}
	${LIBTOOL} --mode=link ${CC} ${CFLAGS} -o $@ ${LINK_$p}
.endfor

.for t in ${PROGS}
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

CLEANFILES += ${PROGS} s1 *.c *.o *.lo *.la .libs/* 
CLEANFILES += ${DEST}/bin/* ${DEST}/lib/* ${DEST2}/lib/*

.PHONY: ${REGRESS_TARGETS} regress

.include <bsd.regress.mk>
