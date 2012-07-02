# $OpenBSD: Makefile,v 1.1 2012/07/02 12:02:36 espie Exp $
REGRESS_TARGETS = test-compile-0 test-link-0 test-install-0 \
	test-run-0 test-link-1 test-install-1 test-run-1 \
	test-link-2 test-link-3 test-run-2 test-run-3

LIBTOOL ?= /usr/bin/libtool

test-compile-0: ${OBJLA}

test-link-0: liba.la

test-link-1: p1

test-run-0: p1
	./p1

test-link-2: liba0.la liba1.la

test-link-3: p2

test-run-2: p2
	./p2

test-install-0: dest/liba.la

test-install-1: dest/p1

test-run-1: dest/p1
	LD_LIBRARY_PATH=dest dest/p1

test-run-3:  dest/p2
	LD_LIBRARY_PATH=dest dest/p2

dest/p1: dest/liba.la
dest/p2: dest/liba0.la dest/liba1.la


# basic framework to build/link stuff
SOPTS = --version-info 0:0:0 --rpath /usr/local/lib

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


FULLLIBS = ${LIBS:S/^/lib/:S/$/.la/}

# stuff to compile/link/install everything
.SUFFIXES: .lo

.c.lo:
	${LIBTOOL} --mode=compile ${CC} -c ${CFLAGS} ${.CURDIR}/$*.c

.for p in ${PROGS}
$p: ${OBJ_$p}
	${LIBTOOL} --mode=link ${CC} ${CFLAGS} -o $@ ${LINK_$p}
.endfor

.for t in ${PROGS} ${FULLLIBS}
dest/$t:
	mkdir -p dest
	${LIBTOOL} --mode=install cp $t dest/$t
.endfor

.for l in ${LIBS}
lib$l.la: ${OBJ_$l}
	${LIBTOOL} --mode=link ${CC} ${CFLAGS} -o $@ ${$l_OPTS} ${OBJ_$l}
.endfor

CLEANFILES += *.o *.lo *.la .libs/* dest/*

.PHONY: ${REGRESS_TARGETS} regress

.include <bsd.regress.mk>
