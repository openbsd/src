#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$OpenBSD: Makefile,v 1.16 2000/09/03 18:41:14 espie Exp $

.include <bsd.own.mk>

SUBDIR=	atrun comsat fingerd ftpd getNAME getty identd lockspool \
	mail.local makewhatis rexecd rlogind rshd \
	rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld rpc.sprayd \
	talkd tcpd telnetd tftpd uucpd smtpd

.if (${YP:L} == "yes")
SUBDIR+=rpc.yppasswdd
.endif

.include <bsd.subdir.mk>
