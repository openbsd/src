#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$OpenBSD: Makefile,v 1.34 2002/12/21 01:41:54 deraadt Exp $

.include <bsd.own.mk>

SUBDIR=	comsat fingerd ftpd ftp-proxy getNAME getty identd lockspool \
	mail.local makewhatis \
	rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld rpc.sprayd spamd \
	rshd talkd tcpd telnetd tftpd uucpd
SUBDIR+=login_passwd login_skey login_krb4 login_krb4-or-pwd login_reject \
	login_chpass login_lchpass login_token login_radius

.if (${YP:L} == "yes")
SUBDIR+=rpc.yppasswdd
.endif

.if (${ELF_TOOLCHAIN} == "yes") && !defined(NOPIC)
SUBDIR+=ld.so
.endif

.if (${KERBEROS5:L} == "yes")
SUBDIR+=login_krb5 login_krb5-or-pwd
.endif

.include <bsd.subdir.mk>
