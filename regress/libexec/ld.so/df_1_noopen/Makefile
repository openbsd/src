# $OpenBSD: Makefile,v 1.6 2014/12/01 01:16:46 deraadt Exp $

.include <bsd.own.mk>

REGRESS_TARGETS = test

.include <bsd.regress.mk>

EMPTYFILE=	emptyfile
CLEANFILES+=	${EMPTYFILE}
LIB_DEPENDS=	${EMPTYFILE}

# gcc3 will error with "file not recognized: File truncated" if the input file
# is not at least four bytes long!
${EMPTYFILE}:
	printf "\n\n\n\n" > $@

CLEANFILES+=lib1.so lib2.so lib3.so dlopen dlopen1 dlopen2 dlopen3

.PHONY: test
test: lib1.so lib2.so lib3.so dlopen dlopen1 dlopen2 dlopen3
	${SHELL} ${.CURDIR}/test.sh

lib1.so: ${LIB_DEPENDS}
	cc -o lib1.so ${EMPTYFILE} -fPIC -shared -Wl,-znodlopen

lib2.so: ${LIB_DEPENDS}
	cc -o lib2.so ${EMPTYFILE} -fPIC -shared -L. -l1

lib3.so: ${LIB_DEPENDS}
	cc -o lib3.so ${EMPTYFILE} -fPIC -shared -L. -l2

dlopen: dlopen.c
	cc -o dlopen ${.CURDIR}/dlopen.c

dlopen1: dlopen.c
	cc -o dlopen1 ${.CURDIR}/dlopen.c -L. -l1

dlopen2: dlopen.c
	cc -o dlopen2 ${.CURDIR}/dlopen.c -L. -l2

dlopen3: dlopen.c
	cc -o dlopen3 ${.CURDIR}/dlopen.c -L. -l3
