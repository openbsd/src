#	$NetBSD: Makefile,v 1.5 1995/02/25 09:02:37 cgd Exp $

LIB=	l
SRCS=	libmain.c libyywrap.c
NOPIC=

LINKS=	${LIBDIR}/libl.a ${LIBDIR}/libfl.a

.PATH:	${.CURDIR}/../../usr.bin/lex
.include <bsd.lib.mk>

.if !defined(NOPROFILE)
LINKS+=	${LIBDIR}/libl_p.a ${LIBDIR}/libfl_p.a
.endif
