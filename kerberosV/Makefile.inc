# $OpenBSD: Makefile.inc,v 1.9 2003/07/14 16:14:43 mho Exp $

KRB5DIR	= ${.CURDIR}/${KRBDEPTH}
KRB5SRC	= ${.CURDIR}/${KRBDEPTH}/src

CFLAGS	+= -DHAVE_CONFIG_H -DBINDIR=\"/usr/bin\" -DSBINDIR=\"/usr/sbin\"\
	   -DLIBEXECDIR=\"/usr/libexec\" -DSYSCONFDIR=\"/etc/kerberosV\" \
	   -I${DESTDIR}/usr/include/kerberosV \
	   -I${KRB5DIR}/include -I${KRB5SRC}/lib/roken\
	   -I${KRB5SRC}/include -I${KRB5SRC}/lib/sl 
CFLAGS+=	-Wall
.include <bsd.own.mk>				# for 'NOPIC' definition
.if !defined(NOPIC)
CFLAGS	+= -DHAVE_DLOPEN
.endif

PRINTVER=	print_version.c
ROKENSRC=	${KRB5SRC}/lib/roken

.PATH:	${KRB5SRC}/man
.PATH:	${KRB5DIR}/openbsd-vers

COMPILE_ET	= /usr/bin/compile_et
