# $OpenBSD: Makefile.common.inc,v 1.3 2003/06/23 14:39:14 mho Exp $

KRB5SRC	= ${KRB5DIR}/src

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

.PATH:	${KRB5SRC}/man
