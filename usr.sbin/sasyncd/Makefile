#	$Id: Makefile,v 1.4 2005/05/23 19:53:27 ho Exp $

PROG=	sasyncd
SRCS=	sasyncd.c carp.c conf.y log.c net.c net_ctl.c pfkey.c timer.c
MAN=	sasyncd.8 sasyncd.conf.5

.ifdef DEBUG
CFLAGS=		-O0 $(PIPE) $(DEBUG)
.endif

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
