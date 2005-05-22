#	$Id: Makefile,v 1.3 2005/05/22 20:35:48 ho Exp $

PROG=	sasyncd
SRCS=	sasyncd.c carp.c conf.y log.c net.c net_ctl.c pfkey.c timer.c
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
LDADD+=		-lcrypto

.include <bsd.prog.mk>
