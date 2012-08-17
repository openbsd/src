#	$Id: Makefile,v 1.9 2012/08/17 17:11:21 haesbaert Exp $

PROG=	sasyncd
SRCS=	sasyncd.c carp.c conf.y log.c monitor.c net.c net_ctl.c pfkey.c timer.c
MAN=	sasyncd.8 sasyncd.conf.5

.ifdef DEBUG
CFLAGS=		-O0 $(PIPE) $(DEBUG)
.endif

CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../../sbin/iked

CLEANFILES=	y.tab.h

# ElectricFence
#LDADD+=	-L/usr/local/lib -lefence

CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes \
		-Wmissing-declarations
LDADD+=		-lcrypto -lutil

.include <bsd.prog.mk>
