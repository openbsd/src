#	$OpenBSD: Makefile.inc,v 1.16 2001/06/26 17:40:42 markus Exp $

CFLAGS+=	-I${.CURDIR}/..

CFLAGS+=	-Wall
#CFLAGS+=	-Werror
CFLAGS+=	-Wpointer-arith
CFLAGS+=	-Wno-uninitialized
#CFLAGS+=	-Wstrict-prototypes
CFLAGS+=	-Wmissing-prototypes

#DEBUG=-g
#CFLAGS+=	-DSMARTCARD

.include <bsd.obj.mk>

.if exists(${.CURDIR}/../lib/${__objdir})
LDADD+=         -L${.CURDIR}/../lib/${__objdir} -lssh
DPADD+=         ${.CURDIR}/../lib/${__objdir}/libssh.a
.else
LDADD+=         -L${.CURDIR}/../lib -lssh
DPADD+=         ${.CURDIR}/../lib/libssh.a
.endif
