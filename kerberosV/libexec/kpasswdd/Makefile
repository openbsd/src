# $OpenBSD: Makefile,v 1.4 2001/09/12 05:16:25 jason Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kpasswdd
MAN=	${PROG}.8

.PATH:	${KRB5SRC}/kpasswd
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkrb -ldes ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lkafs

.include <bsd.prog.mk>

.if (${ELF_TOOLCHAIN:L} == "yes" && ${MACHINE_ARCH} != "sparc64")
LDADD+= -ldl
.endif
