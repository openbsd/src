PROG=	ftp-proxy
SRCS=	ftp-proxy.c filter.c
MAN=	ftp-proxy.8

CFLAGS+= -I${.CURDIR}
CFLAGS+= -Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith \
         -Wno-uninitialized
LDADD+=	-levent

.include <bsd.prog.mk>
