.PATH:		${.CURDIR}/..

PROG=	ssh-add
BINOWN=	root

.if (${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "powerpc" || \
	${MACHINE_ARCH} == "hppa")
BINMODE=0000
.else
BINMODE?=555
.endif

BINDIR=	/usr/bin
LDADD=	-lcrypto -lutil -lz
DPADD=	${LIBCRYPTO} ${LIBDES} ${LIBUTIL} ${LIBZ}
MAN=	ssh-add.1

SRCS=	ssh-add.c log-client.c readpass.c rsa.c buffer.c xmalloc.c \
	bufaux.c authfd.c authfile.c mpaux.c cipher.c

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
