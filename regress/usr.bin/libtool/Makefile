# $OpenBSD: Makefile,v 1.3 2012/07/02 12:40:20 espie Exp $
REGRESS_TARGETS = test-compile-0 test-link-0 test-install-0 \
	test-run-0 test-link-1 test-install-1 test-run-1 \
	test-link-2 test-link-3 test-run-2 test-run-3 \
	error-0 error-1

LIBTOOL ?= /usr/bin/libtool

DEST = ${.OBJDIR}/dest/usr/local

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

error-0: liba.la
# this should error out (not absolute directory)
	if ${LIBTOOL} --mode=install cp liba.la dest; then exit 1; fi

error-1:
	touch dummy.c
	${LIBTOOL} --mode=compile --tag=notatag ${CC} -c -o dummy.lo dummy.c 2>&1|fgrep -q "ignoring unknown tag"

	
${DEST}/bin/p1: ${DEST}/lib/liba.la
${DEST}/bin/p2: ${DEST}/lib/liba0.la ${DEST}/lib/liba1.la

# basic framework to build/link stuff
SOPTS = --version-info 0:0:0 -rpath /usr/local/lib

LIBS = a a0 a1
PROGS = p1 p2

OBJ_a = a.lo b.lo
a_OPTS = ${SOPTS}

OBJ_a0 = a.lo
a0_OPTS = ${SOPTS}

OBJ_a1 = b.lo
a1_OPTS = ${SOPTS} -la0


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
${DEST}/bin/$t:
	mkdir -p ${DEST}/bin
	${LIBTOOL} --mode=install cp $t ${DEST}/bin/$t
.endfor

.for t in ${LIBS}
${DEST}/lib/lib$t.la:
	mkdir -p ${DEST}/lib
	${LIBTOOL} --mode=install cp lib$t.la ${DEST}/lib/lib$t.la
.endfor

.for l in ${LIBS}
lib$l.la: ${OBJ_$l}
	${LIBTOOL} --mode=link ${CC} ${CFLAGS} -o $@ ${$l_OPTS} ${OBJ_$l}
.endfor

CLEANFILES += *.c *.o *.lo *.la .libs/* ${DEST}/bin/* ${DEST}/lib/*

.PHONY: ${REGRESS_TARGETS} regress

.include <bsd.regress.mk>
