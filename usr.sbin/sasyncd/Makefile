#	$Id: Makefile,v 1.2 2005/04/03 17:19:26 ho Exp $

PROG=	sasyncd
SRCS=	sasyncd.c carp.c conf.y log.c net.c net_ctl.c net_ssl.c \
	pfkey.c timer.c
MAN=	sasyncd.8 sasyncd.conf.5

CFLAGS+=	-I${.CURDIR}
CLEANFILES=	y.tab.h

# ElectricFence
#LDADD+=	-L/usr/local/lib -lefence

# Boehms GC
#LDADD+=	-L/usr/local/lib -lgc
#CFLAGS+=	-DGC_DEBUG

CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes \
		-Wmissing-declarations
LDADD+=		-lcrypto -lssl

.include <bsd.prog.mk>
