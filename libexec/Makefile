#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$OpenBSD: Makefile,v 1.15 1998/08/15 21:02:20 millert Exp $

.include <bsd.own.mk>

SUBDIR=	atrun comsat fingerd ftpd getNAME getty identd lockspool \
	mail.local makewhatis rexecd rlogind rshd \
	rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld rpc.sprayd \
	talkd tcpd telnetd tftpd uucpd smtpd

.if (${YP} == "yes")
SUBDIR+=rpc.yppasswdd
.endif

.include <bsd.subdir.mk>
