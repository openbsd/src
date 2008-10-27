#	$OpenBSD: Makefile,v 1.6 2008/10/27 18:02:14 miod Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c file.c headers.c \
	safile.c sock.c pipe.c listen.c wav.c legacy.c 
CFLAGS+= -DDEBUG -Wall -Wstrict-prototypes -Wundef
# false warnings on 64 bit variables for which no toolchain fix is available yet
.if ${MACHINE_ARCH} != "m88k"
CFLAGS+= -Werror
.endif
LDADD+=	-lsndio
.include <bsd.prog.mk>
