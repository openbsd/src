#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$OpenBSD: Makefile,v 1.8 1996/11/11 05:06:32 downsj Exp $

.include <bsd.own.mk>

SUBDIR=	atrun compile_et comsat fingerd ftpd getNAME getty identd \
	lfs_cleanerd mail.local makewhatis rexecd rlogind rshd rpc.lockd \
	rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld rpc.sprayd \
	talkd telnetd tftpd uucpd

.if defined(YP)
SUBDIR+=rpc.yppasswdd
.endif

.include <bsd.subdir.mk>
