#	$OpenBSD: Makefile,v 1.6 2016/07/30 03:25:49 guenther Exp $

OBJS= zbsdmod.o
SRCS= zbsdmod.c
NOMAN=

all: ${OBJS}

clean:
	rm -f zbsdmod.o

.include <bsd.prog.mk>

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o root -g wheel -m 644 \
		${OBJS} ${DESTDIR}${BINDIR}

CFLAGS= -fno-stack-protector -Wall
CFLAGS+= -DMACHINE=\"${MACHINE}\" -DUTS_RELEASE=\"2.4.20\"
CPPFLAGS= -I${.CURDIR}/../include -fno-pie
LDFLAGS+= -nopie -znorelro
