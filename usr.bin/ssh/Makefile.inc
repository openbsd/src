#	$OpenBSD: Makefile.inc,v 1.56 2017/12/10 19:37:57 deraadt Exp $

.include <bsd.own.mk>

CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-fstack-protector-all

CDIAGFLAGS=	-Wall
CDIAGFLAGS+=	-Wextra
CDIAGFLAGS+=	-Wformat-security
CDIAGFLAGS+=	-Wmissing-declarations
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wshadow
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wunused
CDIAGFLAGS+=	-Wno-unused-parameter # Lots of these in protocol handlers.
.if ${COMPILER_VERSION:L} != "gcc3"
CDIAGFLAGS+=	-Wstrict-aliasing=2
CDIAGFLAGS+=	-Wold-style-definition
.endif

#CDIAGFLAGS+=	-Werror
#DEBUG=-g
#INSTALL_STRIP=

WARNINGS=yes

OPENSSL?=	yes

.if (${OPENSSL:L} == "yes")
CFLAGS+=	-DWITH_OPENSSL
.endif

CFLAGS+=	-DENABLE_PKCS11
.ifndef NOPIC
CFLAGS+=	-DHAVE_DLOPEN
.endif

.include <bsd.obj.mk>
