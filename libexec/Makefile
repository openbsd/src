#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$OpenBSD: Makefile,v 1.23 2001/06/25 15:51:36 hin Exp $

.include <bsd.own.mk>

SUBDIR=	atrun comsat fingerd ftpd getNAME getty identd lockspool \
	mail.local makewhatis rexecd rlogind rshd \
	rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld rpc.sprayd \
	talkd tcpd telnetd tftpd uucpd smtpd
SUBDIR+=login_passwd login_skey login_krb4 login_krb4-or-pwd login_reject \
	login_chpass login_lchpass login_token

.if (${YP:L} == "yes")
SUBDIR+=rpc.yppasswdd
.endif
.if (${MACHINE_ARCH} == "powerpc") || (${MACHINE_ARCH} == "alpha")
SUBDIR+=ld.so
.endif

.if (${KERBEROS5:L} == "yes")
SUBDIR+=login_krb5 login_krb5-or-pwd
.endif

.include <bsd.subdir.mk>
