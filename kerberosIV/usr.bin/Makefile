# $OpenBSD: Makefile,v 1.3 2001/06/25 16:31:49 hin Exp $

.include <bsd.own.mk> # for KERBEROS5

SUBDIR=	afslog

.if(${KERBEROS5:L} != "yes")
SUBDIR+=kauth kdestroy kinit klist pagsh
.endif

# these are not built: kftp kstring2key

.include <bsd.subdir.mk>
