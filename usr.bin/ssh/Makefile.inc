#	$OpenBSD: Makefile.inc,v 1.17 2001/06/27 19:29:13 markus Exp $

CFLAGS+=	-I${.CURDIR}/..

CFLAGS+=	-Wall
#CFLAGS+=	-Werror
CFLAGS+=	-Wpointer-arith
CFLAGS+=	-Wno-uninitialized
#CFLAGS+=	-Wstrict-prototypes
CFLAGS+=	-Wmissing-prototypes

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
