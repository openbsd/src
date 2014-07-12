#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$OpenBSD: Makefile,v 1.61 2014/07/12 16:34:24 matthieu Exp $

.include <bsd.own.mk>

SUBDIR= auxcpp comsat fingerd ftpd getty ld.so lockspool login_chpass \
	login_lchpass login_passwd login_radius login_reject \
	login_skey login_tis login_token login_yubikey mail.local \
	rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld \
	security spamd spamd-setup spamlogd talkd

.if (${YP:L} == "yes")
SUBDIR+=rpc.yppasswdd
.endif

.include <bsd.subdir.mk>
