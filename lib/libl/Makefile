#	$NetBSD: Makefile,v 1.5 1995/02/25 09:02:37 cgd Exp $

LIB=	l
SRCS=	libmain.c libyywrap.c
NOPIC=

LINKS=	/usr/lib/libl.a /usr/lib/libfl.a

.PATH:	${.CURDIR}/../../usr.bin/lex
.include <bsd.lib.mk>

.if !defined(NOPROFILE)
LINKS+=	/usr/lib/libl_p.a /usr/lib/libfl_p.a
.endif
