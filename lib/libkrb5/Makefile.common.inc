# $OpenBSD: Makefile.common.inc,v 1.1 2003/05/14 09:05:59 hin Exp $

KRB5SRC	= ${KRB5DIR}/src

KRB4DIR	= ${KRB5DIR}/../kerberosIV
KRB4SRC	= ${KRB4DIR}/src

CFLAGS	+= -DHAVE_CONFIG_H -DBINDIR=\"/usr/bin\" -DSBINDIR=\"/usr/sbin\"\
	   -DLIBEXECDIR=\"/usr/libexec\" -DSYSCONFDIR=\"/etc/kerberosV\" \
	   -I${DESTDIR}/usr/include/kerberosV \
	   -I${DESTDIR}/usr/include/kerberosIV \
	   -I${KRB5DIR}/include -I${KRB5SRC}/lib/roken\
	   -I${KRB5SRC}/include -I${KRB4SRC}/lib/sl 
CFLAGS+=	-Wall

.include <bsd.own.mk>				# for 'NOPIC' definition
.if !defined(NOPIC)
CFLAGS	+= -DHAVE_DLOPEN
.endif

.PATH:	${KRB5SRC}/man

COM_ERR=	-lcom_err_pic
