#	$Id: Makefile,v 1.10 2017/07/03 22:21:47 espie Exp $

PROG=	sasyncd
SRCS=	sasyncd.c carp.c conf.y log.c monitor.c net.c net_ctl.c pfkey.c timer.c
MAN=	sasyncd.8 sasyncd.conf.5

.ifdef DEBUG
CFLAGS=		-O0 $(PIPE) $(DEBUG)
.endif

CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../../sbin/iked

YFLAGS=

# ElectricFence
#LDADD+=	-L/usr/local/lib -lefence

CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes \
		-Wmissing-declarations
LDADD+=		-lcrypto -lutil

.include <bsd.prog.mk>
