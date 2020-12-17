# $OpenBSD: Makefile,v 1.6 2020/12/17 00:51:11 bluhm Exp $

CSH=	/bin/csh

REGRESS_TARGETS=	filec

PROG=		edit
CFLAGS+=	-Wall
LDADD+=		-lutil
CLEANFILES+=	edit.c

edit.c: ${.CURDIR}/../ksh/edit/edit.c
	@cp ${.CURDIR}/../ksh/edit/edit.c .

filec: edit
	sh ${.CURDIR}/filec.sh ${CSH}

REGRESS_TARGETS+=	env

.SUFFIXES: .in

.in:
	env -i MALLOC_OPTIONS=S ${CSH} <${.CURDIR}/${@}.in 2>&1 | \
		diff -u ${.CURDIR}/${@}.ok -

.include <bsd.regress.mk>
