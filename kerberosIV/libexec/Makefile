# $OpenBSD: Makefile,v 1.4 2003/03/21 09:46:37 hin Exp $

.include <bsd.own.mk> # for KERBEROS5

SUBDIR=	kauthd kpropd

.if (${KERBEROS5:L} != "yes")
SUBDIR+=kadmind
.endif

.include <bsd.subdir.mk>
