# $OpenBSD: Makefile,v 1.5 2001/09/27 05:02:57 art Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kpasswdd
MAN=	${PROG}.8

.PATH:	${KRB5SRC}/kpasswd
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkrb -ldes ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lkafs

.include <bsd.prog.mk>

.if (${ELF_TOOLCHAIN:L} == "yes")
LDADD+= -ldl
.endif
