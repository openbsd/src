#	$OpenBSD: Makefile,v 1.1 2011/11/15 23:22:47 gilles Exp $

.PATH:		${.CURDIR}/..

LIB=	smtpdfilter
WANTLINT=no
SRCS=	filter.c filter_api.c
DEBUGLIBS= no
NOPROFILE= yes
NOPIC=	yes

install:
	@echo -n

.include <bsd.own.mk>
.include <bsd.lib.mk>
