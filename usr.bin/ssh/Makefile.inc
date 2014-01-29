#	$OpenBSD: Makefile.inc,v 1.42 2014/01/29 06:18:35 djm Exp $

CFLAGS+=	-I${.CURDIR}/..

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
CDIAGFLAGS+=	-Wold-style-definition
CDIAGFLAGS+=	-Wno-unused-parameter # Not clean for this yet
#CDIAGFLAGS+=	-Wwrite-strings # XXX too messy; needs libc changes
#CDIAGFLAGS+=	-Wcast-qual # XXX not clean
#CDIAGFLAGS+=	-Wcast-align # XXX can't be clean with sockaddr casts?

#CDIAGFLAGS+=	-Werror
#DEBUG=-g

WARNINGS=yes
INSTALL_STRIP=

CFLAGS+=	-DENABLE_PKCS11
.include <bsd.own.mk>
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
