#	$OpenBSD: Makefile.inc,v 1.29 2004/06/21 18:34:51 avsm Exp $

CFLAGS+=	-I${.CURDIR}/..

CDIAGFLAGS=	-Wall
#CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wno-uninitialized
#CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wunused
#CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wbounded
#CDIAGFLAGS+=	-Wshadow

#DEBUG=-g

#CFLAGS+=	-DSMARTCARD
#LDADD+=	-lsectok

.include <bsd.obj.mk>

.if exists(${.CURDIR}/../lib/${__objdir})
LDADD+=         -L${.CURDIR}/../lib/${__objdir} -lssh
DPADD+=         ${.CURDIR}/../lib/${__objdir}/libssh.a
.else
LDADD+=         -L${.CURDIR}/../lib -lssh
DPADD+=         ${.CURDIR}/../lib/libssh.a
.endif
