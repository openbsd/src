.PATH:		${.CURDIR}/..

PROG=	ssh-agent
BINOWN=	root
BINMODE=555
BINDIR=	/usr/bin
LDADD=	-lcrypto -lutil -lz
DPADD=	${LIBCRYPTO} ${LIBDES} ${LIBUTIL} ${LIBZ}
MAN=	ssh-agent.1

SRCS=	ssh-agent.c log-client.c rsa.c buffer.c xmalloc.c bufaux.c \
	authfd.c authfile.c mpaux.c cipher.c

.include <bsd.prog.mk>

.if (${KERBEROS} == "yes")
CFLAGS+= -DKRB4 -I/usr/include/kerberosIV
LDADD+=	 -lkrb
DPADD+=	 ${LIBKRB}
.if (${AFS} == "yes")
CFLAGS+= -DAFS
LDADD+=  -lkafs
DPADD+=  ${LIBKRBAFS}
.endif # AFS
.endif # KERBEROS
