.PATH:		${.CURDIR}/..

PROG=	ssh
BINOWN=	root
BINMODE=4555
BINDIR=	/usr/bin
MAN=	ssh.1
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c sshconnect.c log-client.c readconf.c hostfile.c readpass.c \
	tildexpand.c uidswap.c clientloop.c canohost.c rsa.c \
	buffer.c packet.c xmalloc.c ttymodes.c channels.c bufaux.c \
	authfd.c authfile.c crc32.c match.c mpaux.c cipher.c \
	compress.c

.include <bsd.own.mk> # for AFS

.if (${AFS} == "yes")
SRCS+=	radix.c
.endif

.include <bsd.prog.mk>

LDADD+=	-lutil -lz -lcrypto
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}
