#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$OpenBSD: Makefile,v 1.55 2014/03/12 18:17:26 tedu Exp $

.include <bsd.own.mk>

SUBDIR= comsat fingerd ftpd getty ld.so lockspool login_chpass \
	login_lchpass login_passwd login_radius login_reject \
	login_skey login_tis login_token login_yubikey mail.local \
	makewhatis rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld \
	rshd security spamd spamd-setup spamlogd talkd \
	tcpd uucpd

.if (${YP:L} == "yes")
SUBDIR+=rpc.yppasswdd
.endif

.if (${KERBEROS5:L} == "yes")
SUBDIR+=login_krb5 login_krb5-or-pwd
.endif

.include <bsd.subdir.mk>
