
PROG=	bpflogd
SRCS=	bpflogd.c
SRCS+=	log.c
MAN=	bpflogd.8

LDADD=-lpcap -levent
DPADD=${LIBPCAP} ${LIBEVENT}

WARNINGS=yes

.include <bsd.prog.mk>
