#	$OpenBSD: Makefile,v 1.2 2004/09/28 15:10:50 millert Exp $

.include <bsd.own.mk>

SUBDIR=	lib sudo visudo
.if !defined(NOPIC)
SUBDIR+= noexec
.endif

.include <bsd.subdir.mk>
