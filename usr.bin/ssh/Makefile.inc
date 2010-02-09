#	$OpenBSD: Makefile.inc,v 1.35 2010/02/09 08:55:31 markus Exp $

CFLAGS+=	-I${.CURDIR}/..

CDIAGFLAGS=	-Wall
#CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wno-uninitialized
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wunused
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wbounded
CDIAGFLAGS+=	-Wshadow

#DEBUG=-g

#CFLAGS+=	-DJPAKE

CFLAGS+=	-DENABLE_PKCS11

.include <bsd.obj.mk>

.if exists(${.CURDIR}/../lib/${__objdir})
LDADD+=         -L${.CURDIR}/../lib/${__objdir} -lssh
DPADD+=         ${.CURDIR}/../lib/${__objdir}/libssh.a
.else
LDADD+=         -L${.CURDIR}/../lib -lssh
DPADD+=         ${.CURDIR}/../lib/libssh.a
.endif
