# $OpenBSD: Makefile,v 1.4 2002/06/11 15:26:49 hin Exp $

.include <bsd.own.mk> # for KERBEROS5

.if(${KERBEROS5:L} != "yes")
SUBDIR+=kauth kdestroy kinit klist pagsh
.endif

# these are not built: kftp kstring2key

.include <bsd.subdir.mk>
