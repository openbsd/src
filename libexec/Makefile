#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$OpenBSD: Makefile,v 1.12 1997/04/24 08:23:02 downsj Exp $

.include <bsd.own.mk>

SUBDIR=	atrun comsat fingerd ftpd getNAME getty identd \
	mail.local makewhatis rexecd rlogind rshd rpc.lockd \
	rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld rpc.sprayd \
	talkd tcpd telnetd tftpd uucpd

.if (${YP} == "yes")
SUBDIR+=rpc.yppasswdd
.endif

.include <bsd.subdir.mk>
