#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$OpenBSD: Makefile,v 1.63 2014/07/30 17:21:50 jsg Exp $

.include <bsd.own.mk>

SUBDIR= comsat fingerd ftpd getty ld.so lockspool login_chpass \
	login_lchpass login_passwd login_radius login_reject \
	login_skey login_tis login_token login_yubikey mail.local \
	rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld \
	security spamd spamd-setup spamlogd talkd tradcpp

.if (${YP:L} == "yes")
SUBDIR+=rpc.yppasswdd
.endif

.include <bsd.subdir.mk>
