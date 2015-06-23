#	$OpenBSD: Makefile,v 1.1 2015/06/23 18:03:09 semarie Exp $

HTTPDSRC =	${.CURDIR}/../../../../usr.sbin/httpd

.PATH:	${HTTPDSRC}

REGRESS_TARGETS=	test-patterns

CLEANFILES +=		patterns-tester

#LUA?=			lua53
.ifdef LUA
REGRESS_TARGETS+=	test-patterns-lua
.endif

patterns-tester: patterns-tester.c patterns.c patterns.h
	${CC} -o $@ ${CFLAGS} ${.CURDIR}/patterns-tester.c ${HTTPDSRC}/patterns.c -I${HTTPDSRC}

test-patterns: patterns-tester test-patterns.out test-patterns.in
	cat ${.CURDIR}/test-patterns.in | grep -v '^#' |			\
	while IFS='	' read string pattern comments ; do 			\
		./patterns-tester "$${string}" "$${pattern}" 2>&1 || true;	\
	done | diff -I 'OpenBSD' -u ${.CURDIR}/test-patterns.out -

test-patterns-lua: patterns-tester.lua test-patterns-lua.out test-patterns.in
	cat ${.CURDIR}/test-patterns.in | grep -v '^#' |			\
	while IFS='	' read string pattern comments ; do 			\
		${LUA} ./patterns-tester.lua "$${string}" "$${pattern}" 2>&1 || true;	\
	done | diff -I 'OpenBSD' -u ${.CURDIR}/test-patterns-lua.out -

.include <bsd.regress.mk>
