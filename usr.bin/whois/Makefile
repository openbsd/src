#	$OpenBSD: Makefile,v 1.4 1999/11/26 22:59:21 millert Exp $

PROG=	whois

.if defined(SOCKS)
CFLAGS+=-DSOCKS
CFLAGS+=-Dconnect=Rconnect -Dgetsockname=Rgetsockname -Dlisten=Rlisten \
	-Daccept=Raccept -Drcmd=Rrcmd -Dbind=Rbind -Dselect=Rselect
LDADD+= -lsocks
.endif

.include <bsd.prog.mk>
