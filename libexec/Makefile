#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$OpenBSD: Makefile,v 1.27 2002/05/06 19:48:21 deraadt Exp $

.include <bsd.own.mk>

SUBDIR=	atrun comsat fingerd ftpd ftp-proxy getNAME getty identd lockspool \
	mail.local makewhatis \
	rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld rpc.sprayd \
	rshd talkd tcpd telnetd tftpd uucpd smtpd
SUBDIR+=login_passwd login_skey login_krb4 login_krb4-or-pwd login_reject \
	login_chpass login_lchpass login_token login_radius

.if (${YP:L} == "yes")
SUBDIR+=rpc.yppasswdd
.endif

.if (${MACHINE_ARCH} == "powerpc") || (${MACHINE_ARCH} == "alpha") || \
    (${MACHINE_ARCH} == "sparc64")
SUBDIR+=ld.so
.endif

.if (${KERBEROS5:L} == "yes")
SUBDIR+=login_krb5 login_krb5-or-pwd
.endif

.include <bsd.subdir.mk>
