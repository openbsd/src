#	$OpenBSD: Makefile,v 1.2 2000/04/02 19:48:09 millert Exp $

SUBDIR= libsmutil libsmdb sendmail mailstats makemap praliases smrsh cf/cf
#SUBDIR+= libmilter mail.local rmail vacation
.if make(install)
SUBDIR+= doc/op cf
.endif

.include <bsd.subdir.mk>
