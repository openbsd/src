#	$Id: Makefile,v 1.1 2005/03/30 18:44:49 ho Exp $

PROG=	sasyncd
SRCS=	sasyncd.c carp.c conf.c log.c net.c net_ctl.c net_ssl.c \
	pfkey.c timer.c
MAN=	sasyncd.8 sasyncd.conf.5

# ElectricFence
#LDADD+=	-L/usr/local/lib -lefence

# Boehms GC
#LDADD+=	-L/usr/local/lib -lgc
#CFLAGS+=	-DGC_DEBUG

CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes \
		-Wmissing-declarations
LDADD+=		-lcrypto -lssl

.include <bsd.prog.mk>
