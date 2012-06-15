# $OpenBSD: Makefile,v 1.1 2012/06/15 20:50:06 matthew Exp $

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
	cc -o dlopen dlopen.c

dlopen1: dlopen.c
	cc -o dlopen1 dlopen.c -L. -l1

dlopen2: dlopen.c
	cc -o dlopen2 dlopen.c -L. -l2

dlopen3: dlopen.c
	cc -o dlopen3 dlopen.c -L. -l3

.include <bsd.regress.mk>
