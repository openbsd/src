#	$OpenBSD: Makefile,v 1.4 2003/11/20 23:23:09 avsm Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/4/94

PROG=	ftp-proxy
CFLAGS+=-Wall
SRCS=	ftp-proxy.c getline.c util.c
MAN=	ftp-proxy.8

.include <bsd.own.mk>

.if (${TCP_WRAPPERS:L} == "yes")
CFLAGS+= -DLIBWRAP
LDADD+=	-lwrap
DPADD+=	${LIBWRAP}
.endif

.include <bsd.prog.mk>
