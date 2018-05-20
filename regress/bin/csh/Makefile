# $OpenBSD: Makefile,v 1.5 2017/12/18 19:19:08 anton Exp $

CSH=	/bin/csh

REGRESS_TARGETS=	filec

PROG=		edit
CFLAGS+=	-Wall
LDADD+=		-lutil
CLEANFILES+=	edit.c

edit.c: ${.CURDIR}/../ksh/edit/edit.c
	@cp ${.CURDIR}/../ksh/edit/edit.c .

filec: edit
	@echo '\n======== filec ========'
	sh ${.CURDIR}/filec.sh ${CSH}

REGRESS_TARGETS+=	env

.SUFFIXES: .in

.in:
	@echo '\n======== ${@} ========'
	env -i MALLOC_OPTIONS=S ${CSH} <${.CURDIR}/${@}.in 2>&1 | \
		diff -u ${.CURDIR}/${@}.ok -

.include <bsd.regress.mk>
