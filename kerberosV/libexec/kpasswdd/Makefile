# $OpenBSD: Makefile,v 1.6 2001/09/29 17:45:40 jakob Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kpasswdd
MAN=	${PROG}.8

.PATH:	${KRB5SRC}/kpasswd
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkrb -ldes ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lkafs -lutil

.include <bsd.prog.mk>

.if (${ELF_TOOLCHAIN:L} == "yes")
LDADD+= -ldl
.endif
