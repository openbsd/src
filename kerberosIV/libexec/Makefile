# $OpenBSD: Makefile,v 1.2 2001/06/25 16:31:51 hin Exp $

.include <bsd.own.mk> # for KERBEROS5

SUBDIR=	kauthd kerberos kpropd

.if (${KERBEROS5:L} != "yes")
SUBDIR+=kadmin 
.endif

.include <bsd.subdir.mk>