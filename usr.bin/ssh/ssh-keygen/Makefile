.PATH:		${.CURDIR}/..

PROG=	ssh-keygen
BINOWN=	root
BINMODE=555
BINDIR=	/usr/bin
LDADD=	-lcrypto -lutil -lz
DPADD=	${LIBCRYPTO} ${LIBDES} ${LIBUTIL} ${LIBZ}
MAN=	ssh-keygen.1

SRCS=	ssh-keygen.c log-client.c readpass.c rsa.c buffer.c \
	xmalloc.c authfile.c mpaux.c bufaux.c cipher.c

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
