#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$OpenBSD: Makefile,v 1.10 1997/02/26 06:17:01 downsj Exp $

.include <bsd.own.mk>

SUBDIR=	atrun comsat fingerd ftpd getNAME getty identd \
	lfs_cleanerd mail.local makewhatis rexecd rlogind rshd rpc.lockd \
	rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld rpc.sprayd \
	talkd tcpd telnetd tftpd uucpd

.if defined(YP)
SUBDIR+=rpc.yppasswdd
.endif

.include <bsd.subdir.mk>
