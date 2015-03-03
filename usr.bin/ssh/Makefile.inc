#	$OpenBSD: Makefile.inc,v 1.47 2015/03/03 21:21:13 djm Exp $

.include <bsd.own.mk>

CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-fstack-protector-all

CDIAGFLAGS=	-Wall
CDIAGFLAGS+=	-Wchar-subscripts
CDIAGFLAGS+=	-Wcomment
CDIAGFLAGS+=	-Wextra
CDIAGFLAGS+=	-Wformat
CDIAGFLAGS+=	-Wformat-security
CDIAGFLAGS+=	-Wimplicit
CDIAGFLAGS+=	-Winline
CDIAGFLAGS+=	-Wmissing-declarations
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wparentheses
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wreturn-type
CDIAGFLAGS+=	-Wshadow
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wstrict-aliasing
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wswitch
CDIAGFLAGS+=	-Wtrigraphs
CDIAGFLAGS+=	-Wuninitialized
CDIAGFLAGS+=	-Wunused
.if ${COMPILER_VERSION} == "gcc4"
CDIAGFLAGS+=	-Wold-style-definition
.endif
CDIAGFLAGS+=	-Wno-unused-parameter # Not clean for this yet
#CDIAGFLAGS+=	-Wwrite-strings # XXX too messy; needs libc changes
#CDIAGFLAGS+=	-Wcast-qual # XXX not clean
#CDIAGFLAGS+=	-Wcast-align # XXX can't be clean with sockaddr casts?

#CDIAGFLAGS+=	-Werror
#DEBUG=-g

WARNINGS=yes

SSH1?=		yes
OPENSSL?=	yes

.if (${OPENSSL:L} == "yes")
CFLAGS+=	-DWITH_OPENSSL
.else
# SSH v.1 requires OpenSSL.
SSH1=		no
.endif

.if (${SSH1:L} == "yes")
CFLAGS+=	-DWITH_SSH1
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
