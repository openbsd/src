.PATH:		${.CURDIR}/..

PROG=	sshd
BINOWN=	root
BINMODE=555
BINDIR=	/usr/sbin
MAN=	sshd.8

SRCS=	sshd.c auth-rhosts.c auth-passwd.c auth-rsa.c auth-rh-rsa.c \
	pty.c log-server.c login.c hostfile.c canohost.c \
	servconf.c tildexpand.c uidswap.c serverloop.c rsa.c \
	buffer.c packet.c xmalloc.c ttymodes.c channels.c bufaux.c \
	authfd.c authfile.c crc32.c match.c mpaux.c cipher.c \
	compress.c

.include <bsd.own.mk> # for KERBEROS and AFS

.if (${KERBEROS} == "yes")
SRCS+=	auth-krb4.c
.endif

.if (${AFS} == "yes")
SRCS+=	radix.c
.endif

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}

.if (${TCP_WRAPPERS} == "yes")
CFLAGS+= -DLIBWRAP
LDADD+= -lwrap
DPADD+= ${LIBWRAP}
.endif
