#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$Id: Makefile,v 1.5 1996/09/04 22:52:03 deraadt Exp $

.include <bsd.own.mk>

SUBDIR=	atrun comsat fingerd ftpd getNAME getty identd lfs_cleanerd \
	mail.local makewhatis rexecd rlogind rshd rpc.lockd \
	rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld rpc.sprayd \
	talkd telnetd tftpd uucpd

.if defined(YP)
SUBDIR+=rpc.ypppasswd
.endif

.include <bsd.subdir.mk>
