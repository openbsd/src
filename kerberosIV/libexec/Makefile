# $OpenBSD: Makefile,v 1.3 2001/06/25 18:17:22 angelos Exp $

.include <bsd.own.mk> # for KERBEROS5

SUBDIR=	kauthd kerberos kpropd

.if (${KERBEROS5:L} != "yes")
SUBDIR+=kadmind
.endif

.include <bsd.subdir.mk>
