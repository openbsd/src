#	$OpenBSD: Makefile.inc,v 1.53 2017/04/30 23:17:37 djm Exp $

.include <bsd.own.mk>

CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-fstack-protector-all

CDIAGFLAGS=	-Werror -Wextra -Wall -ftrapv
CDIAGFLAGS+=	-Wextra
CDIAGFLAGS+=	-Wformat-security
CDIAGFLAGS+=	-Winline
CDIAGFLAGS+=	-Wmissing-declarations
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wold-style-definition
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wshadow
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wunused
.if ${COMPILER_VERSION:L} != "gcc3"
CDIAGFLAGS+=	-Wold-style-definition
.endif
CDIAGFLAGS+=	-Wno-unused-parameter # Not clean for this yet
#CDIAGFLAGS+=	-Wwrite-strings # XXX too messy; needs libc changes
#CDIAGFLAGS+=	-Wcast-qual # XXX not clean
#CDIAGFLAGS+=	-Wcast-align # XXX can't be clean with sockaddr casts?

CDIAGFLAGS+=	-Wno-unused-parameter

DEBUG=-g
WARNINGS=yes
INSTALL_STRIP=

SSH1?=		no
OPENSSL?=	yes

.if (${OPENSSL:L} == "yes")
CFLAGS+=	-DWITH_OPENSSL
.endif

CFLAGS+=	-DENABLE_PKCS11
.ifndef NOPIC
CFLAGS+=	-DHAVE_DLOPEN
.endif

.include <bsd.obj.mk>

.if exists(${.CURDIR}/../lib/${__objdir})
LDADD+=         -L${.CURDIR}/../lib/${__objdir} -lssh
DPADD+=         ${.CURDIR}/../lib/${__objdir}/libssh.a
.else
LDADD+=         -L${.CURDIR}/../lib -lssh
DPADD+=         ${.CURDIR}/../lib/libssh.a
.endif

LDADD+=		-lutil
