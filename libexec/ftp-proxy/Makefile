#	$OpenBSD: Makefile,v 1.3 2003/01/23 23:15:16 djm Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/4/94

PROG=	ftp-proxy
CFLAGS+=-Wall -Werror
SRCS=	ftp-proxy.c getline.c util.c
MAN=	ftp-proxy.8

.include <bsd.own.mk>

.if (${TCP_WRAPPERS:L} == "yes")
CFLAGS+= -DLIBWRAP
LDADD+=	-lwrap
DPADD+=	${LIBWRAP}
.endif

.include <bsd.prog.mk>
