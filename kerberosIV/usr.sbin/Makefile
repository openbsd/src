# $OpenBSD: Makefile,v 1.3 2001/06/25 17:54:26 hin Exp $

.include <bsd.own.mk> # for KERBEROS5

SUBDIR=	ext_srvtab kdb_destroy kdb_init kdb_util kdb_edit kstash ksrvutil \
	kprop

.if(${KERBEROS5:L} != "yes")
SUBDIR+=kadmin 
.endif

.include <bsd.subdir.mk>