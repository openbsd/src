# $OpenBSD: Makefile,v 1.3 2017/07/22 13:50:54 anton Exp $

REGRESS_TARGETS=	filec

PROG=		edit
CFLAGS+=	-Wall
LDADD+=		-lutil
CLEANFILES+=	edit.c

edit.c: ${.CURDIR}/../ksh/edit/edit.c
	@cp ${.CURDIR}/../ksh/edit/edit.c .

filec: edit
	sh ${.CURDIR}/filec.sh

.include <bsd.regress.mk>
