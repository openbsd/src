# $OpenBSD: Makefile,v 1.2 2001/06/25 16:31:50 hin Exp $

.include <bsd.own.mk> # for KERBEROS5

SUBDIR=	ext_srvtab kdb_destroy kdb_init kdb_util kdb_edit kstash ksrvutil \
	kprop

.if(${KERBEROS5:L} != "yes")
kadmin 
.endif

.include <bsd.subdir.mk>