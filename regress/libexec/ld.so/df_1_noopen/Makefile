# $OpenBSD: Makefile,v 1.2 2013/01/23 19:46:09 miod Exp $

REGRESS_TARGETS = test

.PHONY: test
test: lib1.so lib2.so lib3.so dlopen dlopen1 dlopen2 dlopen3
	./test.sh

lib1.so:
	cc -o lib1.so /dev/null -fPIC -shared -Wl,-znodlopen

lib2.so:
	cc -o lib2.so /dev/null -fPIC -shared -L. -l1

lib3.so:
	cc -o lib3.so /dev/null -fPIC -shared -L. -l2

dlopen: dlopen.c
	cc -o dlopen ${.CURDIR}/dlopen.c

dlopen1: dlopen.c
	cc -o dlopen1 ${.CURDIR}/dlopen.c -L. -l1

dlopen2: dlopen.c
	cc -o dlopen2 ${.CURDIR}/dlopen.c -L. -l2

dlopen3: dlopen.c
	cc -o dlopen3 ${.CURDIR}/dlopen.c -L. -l3

.include <bsd.regress.mk>
